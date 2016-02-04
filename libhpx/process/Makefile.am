# The process infrastructure
noinst_LTLIBRARIES     = libprocess.la
noinst_HEADERS         = allreduce.h

libprocess_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libprocess_la_CFLAGS   = $(LIBHPX_CFLAGS)
libprocess_la_SOURCES  = broadcast.c process.c termination.c \
                         continuation.c reduce.c allreduce.c \
                         allreduce_actions.c allreduce_glue.c
