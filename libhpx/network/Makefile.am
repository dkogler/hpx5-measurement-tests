if HAVE_NETWORK
SUBDIRS = isir pwc
endif

# The networking library
noinst_LTLIBRARIES     = libnetwork.la
noinst_HEADERS         = inst.h smp.h coalesced_network.h

libnetwork_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libnetwork_la_CFLAGS   = $(LIBHPX_CFLAGS)
libnetwork_la_SOURCES  = locality.c \
                         network.c \
                         parcel.c \
                         parcel_block.c \
                         hpx_parcel_glue.c \
                         smp.c inst.c \
                         coalesced_network.c

if HAVE_NETWORK
libnetwork_la_LIBADD   = isir/libisir.la \
                         pwc/libpwc.la
endif
