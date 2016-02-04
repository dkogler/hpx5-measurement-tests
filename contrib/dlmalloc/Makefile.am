noinst_LTLIBRARIES	    = libmalloc-2.8.6.la

libmalloc_2_8_6_la_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/contrib/dlmalloc -include mmap_wrapper.h $(LIBHPX_CPPFLAGS) -DUSE_LOCKS -DMMAP=dl_mmap_wrapper -DDIRECT_MMAP=dl_mmap_wrapper -DMUNMAP=dl_munmap_wrapper -DHAVE_MREMAP=0 -DHAVE_MORECORE=0 -DDEFAULT_GRANULARITY=4194304 -DNO_SEGMENT_TRAVERSAL=1
libmalloc_2_8_6_la_CFLAGS   = $(LIBHPX_CFLAGS)
libmalloc_2_8_6_la_SOURCES  = malloc-2.8.6.c
