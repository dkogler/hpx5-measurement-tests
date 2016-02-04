# The action registration convenience library
noinst_LTLIBRARIES       = libactions.la
noinst_HEADERS           = 

libactions_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libactions_la_CFLAGS   = $(LIBHPX_CFLAGS)
libactions_la_SOURCES  = init.c marshalled.c vectored.c ffi.c registration.c \
                         call_by_parcel.c exit.c get_handler.c
