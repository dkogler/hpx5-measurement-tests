# -*- autoconf -*---------------------------------------------------------------
# HPX_CONFIG_INSTRUMENTATION
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_CONFIG_INSTRUMENTATION], [
 AC_ARG_ENABLE([instrumentation],
  [AS_HELP_STRING([--enable-instrumentation],
                  [Enable instrumentation @<:@default=no@:>@])],
  [], [enable_instrumentation=no])

 AS_IF([test "x$enable_instrumentation" != xno],
  [AC_DEFINE([ENABLE_INSTRUMENTATION], [1], [Enable instrumentationging stuff])
   AC_DEFINE([ENABLE_PROFILING], [1], [Enable profiling support])
   have_instrumentation=yes])

 AM_CONDITIONAL([ENABLE_INSTRUMENTATION], [test "x$enable_instrumentation" != xno])
])
