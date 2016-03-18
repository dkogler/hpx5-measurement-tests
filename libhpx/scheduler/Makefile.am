SUBDIRS = arch

# The networking library
noinst_LTLIBRARIES       = libscheduler.la
noinst_HEADERS           = cvar.h lco.h thread.h

libscheduler_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libscheduler_la_CFLAGS   = $(LIBHPX_CFLAGS)
libscheduler_la_CXXFLAGS = $(LIBHPX_CXXFLAGS)
libscheduler_la_SOURCES  = cvar.c scheduler.c worker.c thread.c lco.c future.c \
                           and.c stats.c sema.c gencount.c allreduce.c \
                           reduce.c gather.c alltoall.c user_lco.c apex.c \
                           sigmask.c execute_thread.cc
libscheduler_la_LIBADD   = arch/libarch.la
