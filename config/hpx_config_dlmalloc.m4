# -*- autoconf -*---------------------------------------------------------------
# HPX_CONFIG_DLMALLOC([path])
# ------------------------------------------------------------------------------
# Set up HPX to use internal dlmalloc.
#
# Defines
#   HAVE_DLMALLOC
# ------------------------------------------------------------------------------

AC_DEFUN([HPX_CONFIG_DLMALLOC], [
 contrib=$1
 AC_ARG_ENABLE([dlmalloc],
   [AS_HELP_STRING([--enable-dlmalloc],
                   [Use Doug Lea's malloc for GAS allocation @<:@default=no@:>@])],
   [], [enable_dlmalloc=no])

 AS_IF([test "x$enable_dlmalloc" != xno],
   [AC_DEFINE([HAVE_DLMALLOC], [1], [We have dlmalloc])
    LIBHPX_CPPFLAGS="$LIBHPX_CPPFLAGS -DONLY_MSPACES -DUSE_DL_PREFIX -I\$(top_srcdir)/$contrib/"
    LIBHPX_LIBADD="$LIBHPX_LIBADD \$(top_builddir)/$contrib/libmalloc-2.8.6.la"
    have_dlmalloc=yes])
])
