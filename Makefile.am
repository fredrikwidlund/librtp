AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4
AM_CFLAGS = -std=gnu11 -g -O3 -flto -fuse-linker-plugin -Isrc
AM_LDFLAGS = -all-static

EXTRA_DIST = \
CHANGES \
LICENSE \
README.rst

SOURCE_FILES = \
src/rtp.c

HEADER_FILES = \
src/rtp.h

lib_LTLIBRARIES = librtp.la
librtp_la_SOURCES = $(SOURCE_FILES) $(HEADER_FILES)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = librtp.pc

MAINTAINERCLEANFILES = aclocal.m4 config.h.in configure Makefile.in docs/Makefile.in librtp-?.?.?.tar.gz
maintainer-clean-local:; rm -rf autotools m4 librtp-?.?.?

CLEANFILES = {.,src,test}/*.{gcno,gcda,gcov}
