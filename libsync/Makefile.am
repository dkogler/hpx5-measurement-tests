####################
# Makefile for HPX #
####################
SUBDIRS = arch

lib_LTLIBRARIES  = libsync.la

noinst_HEADERS = nop.h
libsync_la_SOURCES  = locks.c sr_barrier.c two_lock_queue.c \
                      chase_lev_ws_deque.c backoff.c lockable_ptr.c \
                      clh_lock.c mcs_lock.c

if HAVE_CMPXCHG16B
  libsync_la_SOURCES += ms_queue.c
endif

libsync_la_CPPFLAGS = -I$(top_srcdir)/include
libsync_la_CFLAGS   = $(HPX_APPS_CFLAGS)
libsync_la_LIBADD   = arch/libarch.la
