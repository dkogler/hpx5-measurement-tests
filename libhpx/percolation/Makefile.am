noinst_LTLIBRARIES = libpercolation.la

libpercolation_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libpercolation_la_CFLAGS   = $(LIBHPX_CFLAGS)

libpercolation_la_SOURCES  = percolation.c

if HAVE_OPENCL
libpercolation_la_SOURCES += opencl.c
endif

