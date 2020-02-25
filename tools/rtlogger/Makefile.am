# $Id$
include $(top_srcdir)/Makefile.am.common

if BUILD_RTLOGGER
SUBDIRS = logserver logcnv prebuilt shared
else
SUBDIRS = shared
endif

DIST_SUBDIRS = $(SUBDIRS)

EXTRA_DIST = logprep/logprep.pl

