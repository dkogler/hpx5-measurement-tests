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

namespace {
using namespace std;

int test_addr_arith_handler() {
  int n1 = 10, n2 = 20;
  auto ptr1 = hpx::alloc_cyclic<uint64_t>(n1, 2);
  auto ptr2 = ptr1 + 5;

  auto dist = ptr2 - ptr1;
  // is dist guaranteed to be > 0?
  cout << "dist: " << dist << endl;

  // this should fail?
  auto ptr4 = ptr1[4];

  hpx::exit(HPX_SUCCESS);
}
HPX_ACTION(HPX_DEFAULT, 0, test_addr_arith, test_addr_arith_handler);

int test_pin_unpin_handler() {
  int n1 = 10;
  auto ptr1 = hpx::alloc_local<int>(n1);
  hpx::pin_guard<int> guard(ptr1);
  int *p = guard.get();
  for (int i = 0; i != n1; i++) {
    p[i] = i;
  }
  hpx::exit(HPX_SUCCESS);
}
HPX_ACTION(HPX_DEFAULT, 0, test_pin_unpin, test_pin_unpin_handler);

int test_subscript_handler() {
  int n1 = 10;
  auto ptr = hpx::alloc_cyclic<uint64_t>(n1, 1);
  hpx::global_ptr<uint64_t> ptr1 = &ptr[2];

  //   uint64_t val = ptr[2]; // not allowed

  hpx::exit(HPX_SUCCESS);
}
HPX_ACTION(HPX_DEFAULT, 0, test_subscript, test_subscript_handler);
}

int main(int argc, char* argv[]) {
  if (int e = hpx::init(&argc, &argv)) {
    std::cerr << "HPX: failed to initialize.\n";
    return e;
  }

  if (int e = hpx::run(&test_addr_arith)) {
    return e;
  }

  if (int e = hpx::run(&test_pin_unpin)) {
    return e;
  }

  if (int e = hpx::run(&test_subscript)) {
    return e;
  }

  hpx::finalize();
  return 0;
}
