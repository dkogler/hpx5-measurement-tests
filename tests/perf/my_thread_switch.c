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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "hpx/hpx.h"


static void _usage(FILE *f, int error) {
  fprintf(f, "Usage: cswitch [options] NUMBER\n"
          "\t-h, show help\n");
  hpx_print_help();
  fflush(f);
  exit(error);
}

static hpx_action_t _cswitch_main = 0;

static int _cswitch_main_action(int *args, size_t size) {
  int n = *args;
  printf("cswitch(%d)\n", n); fflush(stdout);

  for(int i = 0; i < n; i++){
    hpx_thread_yield();
  }
  hpx_exit(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  // register the cswitch action
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _cswitch_main, _cswitch_main_action,
                      HPX_POINTER, HPX_SIZE_T);

  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }

  // parse the command line
  int opt = 0;
  while ((opt = getopt(argc, argv, "h?")) != -1) {
    switch (opt) {
     case 'h':
       _usage(stdout, EXIT_SUCCESS);
     case '?':
     default:
       _usage(stderr, EXIT_FAILURE);
    }
  }

  argc -= optind;
  argv += optind;

  int n = 100000;
  switch (argc) {
   case 0:
     break;
   default:
     _usage(stderr, EXIT_FAILURE);
   case 1:
     n = atoi(argv[0]);
     break;
  }

  // run the main action
  e = hpx_run(&_cswitch_main, &n, sizeof(n));
  hpx_finalize();
  return e;
}

