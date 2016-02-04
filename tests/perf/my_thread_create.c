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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include "hpx/hpx.h"
#include "libhpx/locality.h"
#include "libsync/queues.h"

#define NUM_THREADS 100000
#define ARRAY_SIZE 100
#define BUFFER_SIZE 128

const int DATA_SIZE = sizeof(uint64_t);
const int SET_CONT_VALUE = 1234;

typedef struct initBuffer {
  int  index;
  char message[BUFFER_SIZE];
} initBuffer_t;

static int _initData_handler(const initBuffer_t *args, size_t n) {
 // Get the target of the current thread. The target of the thread is the
 // destination that a parcel was sent to to spawn the current thread.
 // hpx_thread_current_target() returns the address of the thread's target
  hpx_addr_t local = hpx_thread_current_target();
  initBuffer_t *ld = NULL;
  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  ld->index = args->index;
  strcpy(ld->message, args->message);

  hpx_gas_unpin(local);
  //printf("Initialized buffer with index: %u, with message: %s, size
  //of arguments = %d\n", ld->index, ld->message, n);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _initData,
                  _initData_handler, HPX_POINTER, HPX_SIZE_T);

// Test code -- ThreadCreate
static int thread_create_handler(int *args, size_t size) {
  printf("Starting the Threads test\n");
  // Start the timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t addr = hpx_gas_alloc_cyclic(NUM_THREADS, sizeof(initBuffer_t), 0);

  // HPX Threads are spawned as a result of hpx_parcel_send() / hpx_parcel_
  // sync().
  for (int t = 0; t < NUM_THREADS; t++) {
    hpx_addr_t done = hpx_lco_and_new(1);
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(initBuffer_t));

    // Fill the buffer
    initBuffer_t *init = hpx_parcel_get_data(p);
    init->index = t;
    strcpy(init->message, "Thread creation test");

    // Set the target address and action for the parcel
    hpx_parcel_set_target(p, hpx_addr_add(addr, sizeof(initBuffer_t) * t, sizeof(initBuffer_t)));
    hpx_parcel_set_action(p, _initData);

    // Set the continuation target and action for parcel
    hpx_parcel_set_cont_target(p, done);
    hpx_parcel_set_cont_action(p, hpx_lco_set_action);

    // and send the parcel, this spawns the HPX thread
    hpx_parcel_send(p, HPX_NULL);
    hpx_lco_wait(done);

    hpx_lco_delete(done, HPX_NULL);
  }

  hpx_gas_free(addr, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  hpx_exit(HPX_SUCCESS);
}
static HPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED, thread_create, 
                  thread_create_handler, HPX_POINTER, HPX_SIZE_T);

int main(int argc, char *argv[]) {
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }

  // run the main action
  e = hpx_run(&thread_create, &e, sizeof(e));
  printf("gonna finalize\n");
  hpx_finalize();
  return e;
}
