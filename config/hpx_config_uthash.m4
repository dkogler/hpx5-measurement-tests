# -*- autoconf -*---------------------------------------------------------------
# HPX_CONFIG_UTHASH([path])
#
# Appends
#   LIBHPX_CPPFLAGS
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_CONFIG_UTHASH],
  [LIBHPX_CPPFLAGS="$LIBHPX_CPPFLAGS -I\$(top_srcdir)/$1/src"])
