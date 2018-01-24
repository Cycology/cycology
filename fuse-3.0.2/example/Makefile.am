## Process this file with automake to produce Makefile.in
AM_CFLAGS = -Wall -W -Wno-sign-compare -Wstrict-prototypes -Wmissing-declarations -Wwrite-strings -g -O0 -fno-strict-aliasing
AM_CPPFLAGS = -I$(top_srcdir)/include -D_REENTRANT
noinst_HEADERS = ioctl.h
passthroughBlocked_fhdir = .
passthroughBlocked_fh_HEADERS = helper.h vNANDlib.h fuseLogging.h loggingDiskFormats.h logs.h filesystem.h openFile.h cache.h
passthroughBlocked_fh_SOURCES = passthroughBlocked_fh.c helper.c logs.c vNANDlib.c filesystem.c openFile.c cache.c
makefs_SOURCES = makefs.c vNANDlib.c
makeVirtualNAND_SOURCES = makeVirtualNAND.c vNANDlib.c
showNAND_SOURCES = showNAND.c vNANDlib.c
noinst_PROGRAMS = makefs makeVirtualNAND showNAND passthroughBlocked_fh passthrough passthrough_fh null hello hello_ll \
		  ioctl ioctl_client poll poll_client \
		  passthrough_ll notify_inval_inode \
		  notify_store_retrieve notify_inval_entry \
		  cuse cuse_client

LDADD = ../lib/libfuse3.la
passthrough_fh_LDADD = ../lib/libfuse3.la @passthrough_fh_libs@

passthroughBlocked_fh_CPPFLAGS = -O0
passthroughBlocked_fh_LDFLAGS = -O0 -lm

ioctl_client_CPPFLAGS =
ioctl_client_LDFLAGS =
ioctl_client_LDADD =
poll_client_CPPFLAGS =
poll_client_LDFLAGS =
poll_client_LDADD =

EXTRA_DIST = meson.build
