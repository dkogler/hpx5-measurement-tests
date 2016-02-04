# LT Utility libraries
noinst_LTLIBRARIES = libarch.la
noinst_HEADERS     = common/asm_macros.h common/asm.h x86_64/asm.h aarch64/reg_macros.h

libarch_la_CPPFLAGS 	= -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libarch_la_CFLAGS   	= $(LIBHPX_CFLAGS)
libarch_la_CCASFLAGS	= $(LIBHPX_CCASFLAGS)

if CPU_X86_64
libarch_la_SOURCES = x86_64/transfer.S x86_64/asm.S x86_64/frame.c
endif

if CPU_ARM
libarch_la_SOURCES = arm/frame.c arm/asm.S arm/transfer.S
endif

if CPU_AARCH64
libarch_la_SOURCES = aarch64/frame.c aarch64/asm.S aarch64/transfer.S
endif
