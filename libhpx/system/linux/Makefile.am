# LT Utility libraries
noinst_LTLIBRARIES		= liblinux.la

liblinux_la_CPPFLAGS	= -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
liblinux_la_CFLAGS		= $(LIBHPX_CFLAGS)
liblinux_la_SOURCES		= time.c cpu.c mmap.c usleep.c
liblinux_la_LIBADD		= -lrt
