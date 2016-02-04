# The isend-irecv network implementations
noinst_LTLIBRARIES  = libisir.la
noinst_HEADERS      = emulate_pwc.h irecv_buffer.h isend_buffer.h isir.h parcel_utils.h xport.h

libisir_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libisir_la_CFLAGS   = $(LIBHPX_CFLAGS)
libisir_la_SOURCES  = xport.c funneled.c isend_buffer.c irecv_buffer.c \
                      emulate_pwc.c lco_get.c lco_wait.c string.c

if HAVE_MPI
libisir_la_SOURCES += xport_mpi.c
endif
