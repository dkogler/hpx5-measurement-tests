# -*- autoconf -*---------------------------------------------------------------
# HPX_CONFIG_HWLOC([path])
#
# HPX will use the hwloc library to deal with node-local topology. We
# prefer a system-installed version of hwloc overour contrib'ed
# version, but the --with-hwloc option can override this behavior.
#
# Variables
#   with_hwloc
#   have_hwloc
#   build_hwloc
#
# Substitutes
#   LIBHPX_CPPFLAGS
#   LIBHPX_CFLAGS
#   LIBHPX_LIBADD
#   LIBHPX_LIBS
#   HPX_PC_PRIVATE_PKGS
#   HPX_PC_PRIVATE_LIBS
#
# Defines
#   HAVE_HWLOC
# ------------------------------------------------------------------------------
AC_DEFUN([_HAVE_HWLOC], [
 AC_DEFINE([HAVE_HWLOC], [1], [We have hwloc support])
 have_hwloc=yes
])

AC_DEFUN([_HPX_CONTRIB_HWLOC], [
 contrib=$1
 
 # Disable features that are not required for our embedded build
 enable_libxml2=no
 enable_libnuma=no
 enable_opencl=no
 enable_cuda=no
 enable_nvml=no
 enable_gl=no
 HWLOC_SETUP_CORE([$contrib], [_HAVE_HWLOC],
   [AC_MSG_WARN([could not configure hwloc])],
   [1])
 unset enable_libxml2
 unset enable_libnuma
 unset enable_opencl
 unset enable_cuda
 unset enable_nvml
 unset enable_gl

 _HAVE_HWLOC

 # attach the convenience library, cflags, and cppflags to build libhpx.la
 LIBHPX_CFLAGS="$LIBHPX_CFLAGS $HWLOC_EMBEDDED_CFLAGS"
 LIBHPX_CPPFLAGS="$LIBHPX_CPPFLAGS $HWLOC_EMBEDDED_CPPFLAGS"
 LIBHPX_LIBADD="$LIBHPX_LIBADD $HWLOC_EMBEDDED_LDADD"
 LIBHPX_LIBS="$LIBHPX_LIBS $HWLOC_EMBEDDED_LIBS"

 # Export the hwloc flags internally in this case, because some of our tests
 # depend on the LIBHPX headers directly and need to find hwloc.h. This isn't
 # necessary for external apps because hpx.h doesn't have a public dependency on
 # hwloc.
 HPX_APPS_CPPFLAGS="$HPX_APPS_CPPFLAGS $HWLOC_EMBEDDED_CPPFLAGS"
 HPX_APPS_CFLAGS="$HPX_APPS_CFLAGS $HWLOC_EMBEDDED_CFLAGS"

 # Export hwloc lib dependencies to external clients, internal apps get these
 # through the libtool .la infrastructure.
 HPX_PC_PRIVATE_LIBS="$HPX_PC_PRIVATE_LIBS $HWLOC_EMBEDDED_LIBS"
])

AC_DEFUN([_HPX_LIB_HWLOC], [
 # check to see if there is a system-installed version of hwloc
 AC_CHECK_HEADER([hwloc.h],
   [AC_CHECK_LIB([hwloc], [hwloc_topology_init],
     [_HAVE_HWLOC
      LIBHPX_LIBS="$LIBHPX_LIBS -lhwloc"
      HPX_PC_PRIVATE_LIBS="$HPX_PC_PRIVATE_LIBS -lhwloc"])])
])

AC_DEFUN([_HPX_PKG_HWLOC], [
 pkg=$1
 
 # use pkg-config to find an hwloc module
 PKG_CHECK_MODULES([HWLOC], [$pkg],
   [_HAVE_HWLOC
    LIBHPX_CFLAGS="$LIBHPX_CFLAGS $HWLOC_CFLAGS"
    LIBHPX_LIBS="$LIBHPX_LIBS $HWLOC_LIBS"
    HPX_PC_PRIVATE_PKGS="$HPX_PC_PRIVATE_PKGS $pkg"],
   [have_hwloc=no])
])

AC_DEFUN([HPX_CONFIG_HWLOC], [
 contrib=$1
 pkg=$2
 
 # allow the user to override how we find hwloc
 AC_ARG_WITH(hwloc,
   [AS_HELP_STRING([--with-hwloc{=system,PKG,contrib}],
                   [How we find hwloc @<:@default=system@:>@])],
   [], [with_hwloc=yes])

 AS_CASE($with_hwloc,
   [contrib], [build_hwloc=yes],

   # system indicates that we should look for a system-installed hwloc, first
   # in the current path and then as a default-named pkg-config package
   [system], [_HPX_LIB_HWLOC
              AS_IF([test "x$have_hwloc" != xyes], [_HPX_PKG_HWLOC($pkg)])],

   # yes indicates that we should first look for a hwloc in our path, then
   # look for a default-named pkg-config package, and then finally resort to the 
   # contribed version of hwloc
   [yes], [_HPX_LIB_HWLOC
           AS_IF([test "x$have_hwloc" != xyes], [_HPX_PKG_HWLOC($pkg)])
           AS_IF([test "x$have_hwloc" != xyes], [build_hwloc=yes])],

   # any other string is interpreted as a custom pkg-config package name to use
   [_HPX_PKG_HWLOC($with_hwloc)])

 # If we want the contribed HWLOC then configure and build it. We do this
 # with the test (rather than in the case above) because we can't have the
 # HWLOC_SETUP_CORE macro appear more than once---even conditionally.
 AS_IF([test "x$build_hwloc" == xyes], [_HPX_CONTRIB_HWLOC($contrib)])

 AS_IF([test "x$have_hwloc" != xyes],
   [AC_MSG_ERROR([Failed to find hwloc for --with-hwloc=$with_hwloc])])
])
