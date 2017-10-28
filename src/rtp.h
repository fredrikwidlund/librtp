#ifndef RTP_H_INCLUDED
#define RTP_H_INLCUDED

#define RTP_MAX_DISTANCE   10
#define RTP_MAX_DATA_COUNT 256
#define RTP_MAX_HISTORY    100

typedef struct rtp rtp;
typedef struct rtp_fec rtp_fec;
typedef struct rtp_receiver rtp_receiver;

enum rtp_type
{
  RTP_TYPE_DATA,
  RTP_TYPE_FEC
};

struct rtp_fec
{
  uint16_t    snbase_low_bits;
  uint16_t    length_recovery;
  unsigned    e:1;
  unsigned    pt_recovery:7;
  unsigned    mask:24;
  uint32_t    ts_recovery;
  unsigned    x:1;
  unsigned    d:1;
  unsigned    type:3;
  unsigned    index:3;
  uint8_t     offset;
  uint8_t     na;
  uint8_t     snbase_ext_bits;
};

struct rtp
{
  unsigned    v:2;
  unsigned    p:1;
  unsigned    x:1;
  unsigned    cc:4;
  unsigned    m:1;
  unsigned    pt:7;
  uint16_t    sequence_number;
  uint32_t    timestamp;
  uint32_t    ssrc;
  uint32_t   *csrc;
  uint16_t    extension_id;
  uint16_t    extension_length;
  void       *extension;
  void       *data;
  size_t      size;
  rtp_fec     fec;
};

struct rtp_receiver
{
  list       data;
  size_t     data_count;
  rtp      **data_iterator;
  list       fec;
  size_t     fec_count;
};

rtp     *rtp_new(void *, size_t);
void     rtp_delete(rtp *);

ssize_t  rtp_fec_construct(rtp_fec *, void *, size_t);

void     rtp_receiver_construct(rtp_receiver *);
void     rtp_receiver_destruct(rtp_receiver *);
ssize_t  rtp_receiver_write(rtp_receiver *, void *, size_t, int);
ssize_t  rtp_receiver_read(rtp_receiver *, void **, size_t *);

#endif /* RTP_H_INCLUDED */
