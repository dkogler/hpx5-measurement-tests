# The AGAS library
noinst_LTLIBRARIES  = libagas.la

noinst_HEADERS      = agas.h btt.h chunk_table.h gva.h
libagas_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libagas_la_CFLAGS   = $(LIBHPX_CFLAGS)
libagas_la_SOURCES  = agas.c btt.cc chunk_table.cc string.c local.c lva.c free.c

if HAVE_JEMALLOC
libagas_la_SOURCES += jemalloc_cyclic.c jemalloc_global.c
endif

if HAVE_TBBMALLOC
noinst_LTLIBRARIES         += libtbbmalloc.la
libtbbmalloc_la_CPPFLAGS    = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libtbbmalloc_la_CXXFLAGS    = $(LIBHPX_CXXFLAGS)
libtbbmalloc_la_SOURCES     = tbbmalloc_cyclic.cc tbbmalloc_global.cc
libagas_la_LIBADD           = libtbbmalloc.la
endif

if HAVE_DLMALLOC
libagas_la_SOURCES += dlmalloc.c
endif
