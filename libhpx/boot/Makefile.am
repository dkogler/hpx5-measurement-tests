# LT Utility libraries
noinst_LTLIBRARIES	  = libboot.la

# libboot files and flags
libboot_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libboot_la_CFLAGS   = $(LIBHPX_CFLAGS)
libboot_la_SOURCES  = boot.c smp.c 

if HAVE_MPI
libboot_la_CFLAGS  += @MPI_CFLAGS@
libboot_la_SOURCES += mpi.c
libboot_la_LIBADD   = @MPI_LIBS@
endif

if HAVE_PMI
libboot_la_CFLAGS  += @PMI_CFLAGS@
libboot_la_SOURCES += pmi.c
libboot_la_LIBADD   = @PMI_LIBS@
endif
