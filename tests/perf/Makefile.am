noinst_PROGRAMS  = $(TESTS)

AM_CPPFLAGS                     = $(HPX_APPS_CPPFLAGS) -I$(top_srcdir)/include
AM_CFLAGS                       = $(HPX_APPS_CFLAGS) -Wno-unused
AM_LDFLAGS                      = $(HPX_APPS_LDFLAGS) -no-install
LDADD                           = $(HPX_APPS_LDADD)

TESTS = gas_alloc           \
        gas_addr_trans      \
        lco_sema            \
        lco_future          \
        collbench           \
        parbench            \
        thread_switch

if ENABLE_LENGTHY_TESTS
TESTS += lco_and sendrecv mem_alloc
endif

LOG_COMPILER = $(TESTS_CMD)

gas_alloc_SOURCES               = gas_alloc.c
mem_alloc_SOURCES               = mem_alloc.c
gas_addr_trans_SOURCES          = gas_addr_trans.c
lco_and_SOURCES                 = lco_and.c
lco_sema_SOURCES                = lco_sema.c
lco_future_SOURCES              = lco_future.c
sendrecv_SOURCES                = sendrecv.c
collbench_SOURCES               = collbench.c
parbench_SOURCES                = parbench.c
thread_switch_SOURCES           = thread_switch.c

gas_alloc_DEPENDENCIES          = $(HPX_APPS_DEPS)
mem_alloc_DEPENDENCIES          = $(HPX_APPS_DEPS)
gas_addr_trans_DEPENDENCIES     = $(HPX_APPS_DEPS)
lco_and_DEPENDENCIES            = $(HPX_APPS_DEPS)
lco_sema_DEPENDENCIES           = $(HPX_APPS_DEPS)
lco_future_DEPENDENCIES         = $(HPX_APPS_DEPS)
sendrecv_DEPENDENCIES           = $(HPX_APPS_DEPS)
collbench_DEPENDENCIES          = $(HPX_APPS_DEPS)
parbench_DEPENDENCIES           = $(HPX_APPS_DEPS)
thread_switch_DEPENDENCIES      = $(HPX_APPS_DEPS)
