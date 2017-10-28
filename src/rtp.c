#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <dynamic.h>

#include "rtp.h"

int rtp_construct(rtp *rtp, void *data, size_t size)
{
  stream s;
  uint32_t v;
  uint8_t *p;
  size_t i, n;
  int valid;

  *rtp = (struct rtp) {0};
  stream_construct(&s, data, size);
  v = stream_read16(&s);
  rtp->v = stream_read_bits(v, 16, 0, 2);
  rtp->p = stream_read_bits(v, 16, 2, 1);
  rtp->x = stream_read_bits(v, 16, 3, 1);
  rtp->cc = stream_read_bits(v, 16, 4, 4);
  rtp->m = stream_read_bits(v, 16, 8, 1);
  rtp->pt = stream_read_bits(v, 16, 9, 7);
  rtp->sequence_number = stream_read16(&s);
  rtp->timestamp = stream_read32(&s);
  rtp->ssrc = stream_read32(&s);
  if (rtp->cc)
    {
      rtp->csrc = calloc(rtp->cc, sizeof *rtp->csrc);
      for (i = 0; i < rtp->cc; i ++)
        rtp->csrc[i] = stream_read32(&s);
    }
  if (rtp->x)
    {
      rtp->extension_id = stream_read16(&s);
      rtp->extension_length = stream_read16(&s);
      if (rtp->extension_length > stream_size(&s))
        {
          stream_destruct(&s);
          return -1;
        }
      rtp->extension = malloc(rtp->extension_length);
      stream_read(&s, rtp->extension, rtp->extension_length);
    }

  p = stream_data(&s);
  n = stream_size(&s);
  valid = stream_valid(&s);
  stream_destruct(&s);
  if (!valid)
    return -1;
  if (rtp->p)
    {
      if (!n || p[n - 1] > n)
        return -1;
      n -= p[n - 1];
    }
  rtp->size = n;
  rtp->data = malloc(rtp->size);
  if (!rtp->data)
    abort();
  memcpy(rtp->data, p, rtp->size);

  return 0;
}

void rtp_destruct(rtp *rtp)
{
  free(rtp->csrc);
  free(rtp->extension);
  free(rtp->data);
}

rtp *rtp_new(void *data, size_t size)
{
  rtp *rtp;
  int e;

  rtp = malloc(sizeof *rtp);
  if (!rtp)
    abort();
  e = rtp_construct(rtp, data, size);
  if (e == -1)
    {
      rtp_delete(rtp);
      return NULL;
    }
  return rtp;
}

void rtp_delete(rtp *rtp)
{
  rtp_destruct(rtp);
  free(rtp);
}

static int16_t rtp_distance(rtp *a, rtp *b)
{
  return b->sequence_number - a->sequence_number;
}

ssize_t rtp_fec_construct(rtp_fec *fec, void *data, size_t size)
{
  stream s;
  uint32_t v;
  int valid;

  stream_construct(&s, data, size);
  fec->snbase_low_bits = stream_read16(&s);
  fec->length_recovery = stream_read16(&s);
  v = stream_read32(&s);
  fec->e = stream_read_bits(v, 32, 0, 1);
  fec->pt_recovery = stream_read_bits(v, 32, 1, 7);
  fec->mask = stream_read_bits(v, 32, 8, 24);
  fec->ts_recovery = stream_read32(&s);
  v = stream_read32(&s);
  fec->x = stream_read_bits(v, 32, 0, 1);
  fec->d = stream_read_bits(v, 32, 1, 1);
  fec->type = stream_read_bits(v, 32, 2, 3);
  fec->index = stream_read_bits(v, 32, 5, 3);
  fec->offset = stream_read_bits(v, 32, 8, 8);
  fec->na = stream_read_bits(v, 32, 16, 8);
  fec->snbase_ext_bits = stream_read_bits(v, 32, 24, 8);

  valid = stream_valid(&s);
  stream_destruct(&s);
  
  return valid ? 0 : -1;
}

static int16_t rtp_fec_distance(rtp *a, rtp *b)
{
  return b->fec.snbase_low_bits - a->fec.snbase_low_bits;
}

static int rtp_fec_old(rtp *f, uint16_t sn)
{
  uint16_t snlast;

  snlast = f->fec.snbase_low_bits + (f->fec.offset * (f->fec.na - 1));
  return (int16_t)(sn - snlast) >= 0;
}

void rtp_receiver_construct(rtp_receiver *r)
{
  *r = (rtp_receiver) {0};
  list_construct(&r->data);
  list_construct(&r->fec);
}

void rtp_receiver_destruct(rtp_receiver *r)
{
  (void) r;
}

static int rtp_receiver_enqueue_fec(rtp_receiver *r, rtp *f)
{
  rtp **i;
  ssize_t n;

  if (f->p || f->x || f->m || f->cc || f->pt != 96)
    return -1;
  
  n = rtp_fec_construct(&f->fec, f->data, f->size);
  if (n == -1)
    return -1;

  if (f->fec.e != 1 || f->fec.mask || f->fec.x || f->fec.type || f->fec.index || f->fec.snbase_ext_bits)
    return -1;

  if (rtp_fec_old(f, (*(r->data_iterator))->sequence_number))
    return 0;

  list_foreach_reverse(&r->fec, i)
    if (rtp_fec_distance(*i, f) >= 0)
      break;

  list_insert(list_next(i), &f, sizeof f);
  r->fec_count ++;

  return 1;
}

static int rtp_receiver_enqueue_data(rtp_receiver *r, rtp *f)
{
  rtp **i;
  int16_t d;

  if (f->p || f->x || f->m || f->cc)
    {
      fprintf(stderr, "invalid frame\n");
      return -1;
    }
  
  if (r->data_count >= RTP_MAX_DATA_COUNT)
    {
      fprintf(stderr, "count %lu\n", r->data_count);
      return -1;
    }
  list_foreach_reverse(&r->data, i)
    {
      d = rtp_distance(*i, f);
      if (d > RTP_MAX_DISTANCE || d < -RTP_MAX_DISTANCE)
        {
          fprintf(stderr, "d %d\n", d);
          return -1;
        }
      if (d > 0)
        break;
      if (!d)
        return 0;
    }

  list_insert(list_next(i), &f, sizeof f);
  r->data_count ++;
  return 1;
}

ssize_t rtp_receiver_write(rtp_receiver *r, void *data, size_t size, int type)
{
  rtp *f;
  int e;
  
  (void) type;
  
  f = rtp_new(data, size);
  if (!f)
    return -1;

  switch (type)
    {
    case RTP_TYPE_DATA:
      e = rtp_receiver_enqueue_data(r, f);
      break;
    case RTP_TYPE_FEC:
      e = rtp_receiver_enqueue_fec(r, f);
      break;
    default:
      e = -1;
      break;
    }

  if (e <= 0)
    rtp_delete(f);
  if (e == -1)
    return -1;
  return size;
}

static void rtp_receiver_data_release(void *object)
{
  rtp_delete(*(rtp **) object);
}

static void rtp_receiver_flush(rtp_receiver *r)
{
  rtp **i;

  if (!r->data_iterator)
    return;

  while (!list_empty(&r->data))
    {
      i = list_front(&r->data);
      if (rtp_distance(*i, *(r->data_iterator)) <= RTP_MAX_HISTORY)
        break;
      list_erase(i, rtp_receiver_data_release);
      r->data_count --;
    }
  
  while (!list_empty(&r->fec))
    {
      i = list_front(&r->fec);
      if (!rtp_fec_old(*i, (*(r->data_iterator))->sequence_number))
        break;
      list_erase(i, rtp_receiver_data_release);
      r->fec_count --;
    }
}

ssize_t rtp_receiver_read(rtp_receiver *r, void **data, size_t *size)
{
  rtp **i;

  if (list_empty(&r->data))
    return 0;
  
  if (!r->data_iterator)
    i = list_front(&r->data);
  else
    {
      i = list_next(r->data_iterator);
      if (i == list_end(&r->data))
        return 0;

      if (rtp_distance(*(r->data_iterator), *i) != 1)
        return 0;
    }

  r->data_iterator = i;
  rtp_receiver_flush(r);
  *data = (*i)->data;
  *size = (*i)->size;
  return *size;
}
