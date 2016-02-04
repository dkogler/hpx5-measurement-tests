// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <hpx/hpx.h>
#include <libhpx/profiling.h>

static void _usage(FILE *stream) {
  fprintf(stream, "Usage: time_gas_addr_trans [options]\n"
          "\t-h, this help display\n");
  hpx_print_help();
  fflush(stream);
}

static hpx_action_t _main = 0;

#define BENCHMARK "HPX COST OF GAS ADDRESS TRANSLATION (ms)"

#define HEADER "# " BENCHMARK "\n"

#define TEST_BUF_SIZE (1024*64) //64k
#define FIELD_WIDTH 20

static int num[] = {
  100,
  200
};

static void _address_translation_action(int n) {
  int tag;
  for(int i = 0; i < num[0]; i++){
    prof_start_timing("deep_test", &tag);
    prof_stop_timing("deep_test", &tag);
  }
}

static int _main_action(void *args, size_t n) {
  for(int i = 0; i < num[1]; i++){
    _address_translation_action(0);
  }
  hpx_exit(HPX_SUCCESS);
}

int
main(int argc, char *argv[]) {
  // register the actions
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _main, _main_action, HPX_POINTER, HPX_SIZE_T);

  if (hpx_init(&argc, &argv)) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return 1;
  }

  int opt = 0;
  while ((opt = getopt(argc, argv, "h?")) != -1) {
    switch (opt) {
     case 'h':
      _usage(stdout);
      return 0;
     case '?':
     default:
      _usage(stderr);
     return -1;
    }
  }

  // run the main action
  int e = hpx_run(&_main, NULL, 0);
  hpx_finalize();
  return e;
}
