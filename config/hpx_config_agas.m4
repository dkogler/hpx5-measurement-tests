# -*- autoconf -*---------------------------------------------------------------
# HPX_CONFIG_AGAS
#
# Variables
#  have_agas
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_CONFIG_AGAS], [
 AC_ARG_ENABLE([agas],
   [AS_HELP_STRING([--enable-agas], [Enable AGAS (requires C++11) @<:@default=no@:>@])],
   [], [enable_agas=no])

 AS_IF([test "x$enable_agas" != xno],
   [AC_DEFINE([HAVE_AGAS], [1], [We have agas])
    have_agas=yes])  
])
