// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
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

#include <iostream>
#include <hpx/hpx++.h>

using namespace std;

// static int _my_interrupt_handler(void) {
//   printf("Hi, I am an interrupt!\n");
//   return HPX_SUCCESS;
// }
// static HPX_ACTION(HPX_INTERRUPT, 0, _my_interrupt, _my_interrupt_handler);
//
// static int _my_task_handler(void) {
//   printf("Hi, I am a task!\n");
//   hpx_call_cc(HPX_HERE, _my_interrupt, NULL, NULL);
//   return HPX_SUCCESS;
// }
// static HPX_ACTION(HPX_TASK, 0, _my_task, _my_task_handler);

static int _my_action_handler(void) {
  printf("Hi, I am an action!\n");
//   hpx_call_cc(HPX_HERE, _my_task, NULL, NULL);
  return HPX_SUCCESS;
}
auto mah = hpx::make_action(_my_action_handler);

static int _my_typed_handler(int i, float f, char c) {
  printf("Hi, I am a typed action with args: %d %f %c!\n", i, f, c);
  int r;
  mah.call_sync(HPX_HERE, r);
  //   hpx_call_cc(HPX_HERE, _my_action_handler_action_struct::id, NULL, NULL);
  return HPX_SUCCESS;
}
auto mth = hpx::make_action(_my_typed_handler);

int hello(int a) {
  std::cout << "Rank#" << hpx_get_my_rank() << " received " << a << "." << std::endl;
  return HPX_SUCCESS;
}
auto h_act = hpx::make_action(hello);

hpx_status_t test1(int arg) {
  int r;
  h_act.call_sync(HPX_HERE, r, arg);
  
  return HPX_SUCCESS;
}

int main_act(int arg) {
  std::cout << "main action begin..." << std::endl;

  test1(arg);

  int r, i = 1; float f = 3.0; char c = 'b';
  mth.call_sync(HPX_HERE, r, i, f, c);

  hpx::exit(HPX_SUCCESS);
}
auto ma = hpx::make_action(main_act);

int main(int argc, char* argv[]) {

  int e = hpx::init(&argc, &argv);
  if (e) {
    std::cerr << "HPX: failed to initialize." << std::endl;
    return e;
  }
  int a = hpx_get_my_rank() + 1;

  ma.run(a);
  
  hpx::finalize();
  return 0;
}
