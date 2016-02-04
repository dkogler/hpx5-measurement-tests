# LT Utility libraries
noinst_LTLIBRARIES	  = libmemory.la

# libboot files and flags
libmemory_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libmemory_la_CFLAGS   = $(LIBHPX_CFLAGS)
libmemory_la_SOURCES  = glue.c

if HAVE_NETWORK
if HAVE_JEMALLOC
libmemory_la_SOURCES += jemalloc.c
endif
endif

if HAVE_NETWORK
if HAVE_DLMALLOC
libmemory_la_SOURCES  += dlmalloc.c
endif
endif

# Completely separate CXX dependency here via intermediate libtool library,
# otherwise we get libstdc++ dependency in libhpx even if we don't compile any
# C++ files (it uses CXXLD to link libmemory).
if HAVE_NETWORK
if HAVE_TBBMALLOC
noinst_LTLIBRARIES		   += libtbbmalloc.la
libtbbmalloc_la_CPPFLAGS	= -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libtbbmalloc_la_CXXFLAGS 	= $(LIBHPX_CXXFLAGS)
libtbbmalloc_la_SOURCES  	= tbbmalloc.cc
libmemory_la_LIBADD         = libtbbmalloc.la
endif
endif
