SUBDIRS       =
DIST_SUBDIRS  = libffi hwloc-1.11.0 photon libcuckoo dlmalloc
EXTRA_DIST    = jemalloc libffi-mic uthash valgrind

if BUILD_LIBFFI
SUBDIRS      += $(LIBFFI_CONTRIB_DIR)
endif

if BUILD_HWLOC
SUBDIRS      += hwloc-1.11.0
endif

if BUILD_PHOTON
SUBDIRS      += photon
endif

if BUILD_JEMALLOC
SUBDIRS      += jemalloc
endif

if BUILD_LIBCUCKOO
SUBDIRS      += libcuckoo
endif

if HAVE_DLMALLOC
SUBDIRS      += dlmalloc
endif

check-recursive: all-recursive
	echo "HPX ignore above warnings, we do not check contrib projects"
