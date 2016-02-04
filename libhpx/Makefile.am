####################
# Makefile for HPX #
####################
SUBDIRS = system util boot actions memory gas network scheduler process \
          instrumentation percolation

# The main libhpx library
lib_LTLIBRARIES	   = libhpx.la

libhpx_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libhpx_la_CFLAGS   = $(LIBHPX_CFLAGS)
libhpx_la_CXXFLAGS = $(LIBHPX_CXXFLAGS)
libhpx_la_SOURCES  = libhpx.c hpx.c call.c par.c debug.c version.c

libhpx_la_LDFLAGS  = $(LIBHPX_LDFLAGS)
libhpx_la_LIBADD   = $(LIBHPX_LIBS) $(LIBHPX_LIBADD) \
                     $(top_builddir)/libsync/libsync.la \
                     system/libsystem.la \
                     util/libutil.la \
                     boot/libboot.la \
                     actions/libactions.la \
                     memory/libmemory.la \
                     gas/libgas.la \
                     network/libnetwork.la \
                     scheduler/libscheduler.la \
                     process/libprocess.la \
                     instrumentation/libinstrumentation.la \
                     percolation/libpercolation.la

libhpx_la_DEPENDENCIES = $(LIBHPX_LIBADD) \
                         $(top_builddir)/libsync/libsync.la \
                         system/libsystem.la \
                         util/libutil.la \
                         boot/libboot.la \
                         actions/libactions.la \
                         memory/libmemory.la \
                         gas/libgas.la \
                         network/libnetwork.la \
                         scheduler/libscheduler.la \
                         process/libprocess.la \
                         instrumentation/libinstrumentation.la \
                         percolation/libpercolation.la
