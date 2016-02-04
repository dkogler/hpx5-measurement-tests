# -*- autoconf -*---------------------------------------------------------------
# HPX_CONFIG_LIBHPX
#
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_CONFIG_LIBHPX], [
  HPX_PC_PRIVATE_LIBS="$HPX_PC_PRIVATE_LIBS -lstdc++"
  LIBHPX_CPPFLAGS="$LIBHPX_CPPFLAGS"
  LIBHPX_CXXFLAGS="-D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS $LIBHPX_CXXFLAGS -fno-rtti"
  LIBHPX_LIBS="$LIBHPX_LIBS -lstdc++"
  HPX_APPS_LIBS="$HPX_APPS_LIBS -lstdc++"

  # Special handling for libsync. Maybe we should make a libsync.pc?
  HPX_PC_PRIVATE_LIBS="$HPX_PC_PRIVATE_LIBS -lsync"
  
  # Substitute the variables required for pkg-config linking of libhpx
  # externally. Note if we're only building libhpx.a then the --libs output must
  # contain all private dependencies as well.
  AS_IF([test "x$enable_shared" != xyes],
    [HPX_PC_PUBLIC_LIBS="$HPX_PC_PUBLIC_LIBS $HPX_PC_PRIVATE_LIBS"
     HPX_PC_REQUIRES_PKGS="$HPX_PC_REQUIRES_PKGS $HPX_PC_PRIVATE_PKGS"])

  # do substitution for the test and example makefiles
  HPX_APPS_LDADD="\$(top_builddir)/libhpx/libhpx.la"
  HPX_APPS_DEPS="\$(top_builddir)/libsync/libsync.la \$(top_builddir)/libhpx/libhpx.la"
])
