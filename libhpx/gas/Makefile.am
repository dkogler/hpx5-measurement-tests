SUBDIRS =

# LT Utility libraries
noinst_LTLIBRARIES = libgas.la

EXTRA_DIST = smp/smp.h

# libgas files and flags
libgas_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libgas_la_CFLAGS   = $(LIBHPX_CFLAGS)
libgas_la_SOURCES  = hpx_gas_glue.c \
                     gas.c \
                     gpa.c \
                     smp/smp.c \
                     bcast.c
libgas_la_LIBADD   = 


if HAVE_NETWORK
SUBDIRS           += pgas
libgas_la_LIBADD  += pgas/libpgas.la
endif

if HAVE_NETWORK
if HAVE_AGAS
SUBDIRS           += agas
libgas_la_LIBADD  += agas/libagas.la
endif
endif
