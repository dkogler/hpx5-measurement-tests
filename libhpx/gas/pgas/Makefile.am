# The PGAS library
noinst_LTLIBRARIES  = libpgas.la

noinst_HEADERS = cyclic.h heap.h global.h pgas.h
libpgas_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libpgas_la_CFLAGS   = $(LIBHPX_CFLAGS)
libpgas_la_SOURCES  = actions.c pgas.c heap.c string.c

if HAVE_JEMALLOC
libpgas_la_SOURCES += jemalloc_global.c jemalloc_cyclic.c
endif

if HAVE_TBBMALLOC
noinst_LTLIBRARIES		   += libtbbmalloc.la
libtbbmalloc_la_CPPFLAGS	= -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libtbbmalloc_la_CXXFLAGS 	= $(LIBHPX_CXXFLAGS)
libtbbmalloc_la_SOURCES  	= tbbmalloc_cyclic.cc tbbmalloc_global.cc
libpgas_la_LIBADD           = libtbbmalloc.la
endif

if HAVE_DLMALLOC
libpgas_la_SOURCES += dlmalloc.c
endif
