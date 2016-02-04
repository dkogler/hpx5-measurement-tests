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

#ifndef LIBHPX_THREAD_SIGMASK_H
#define LIBHPX_THREAD_SIGMASK_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <signal.h>
#include <libhpx/debug.h>
#include <libhpx/parcel.h>
#include <libhpx/scheduler.h>
#include "thread.h"

int hpx_thread_sigmask(int how, int mask) {
  worker_t *w = self;
  hpx_parcel_t *p = w->current;
  p->ustack->masked = 1;

  sigset_t set;
  sigemptyset(&set);
  if (mask & HPX_SIGSEGV) {
    sigaddset(&set, SIGSEGV);
  }

  if (mask & HPX_SIGABRT) {
    sigaddset(&set, SIGABRT);
  }

  if (mask & HPX_SIGFPE) {
    sigaddset(&set, SIGFPE);
  }

  if (mask & HPX_SIGILL) {
    sigaddset(&set, SIGILL);
  }

  if (mask & HPX_SIGBUS) {
    sigaddset(&set, SIGBUS);
  }

  if (mask & HPX_SIGIOT) {
    sigaddset(&set, SIGIOT);
  }

  if (mask & HPX_SIGSYS) {
    sigaddset(&set, SIGSYS);
  }

  if (mask & HPX_SIGTRAP) {
    sigaddset(&set, SIGTRAP);
  }

  mask = HPX_SIGNONE;

  if (how == HPX_SIG_BLOCK) {
    how = SIG_BLOCK;
  }

  if (how == HPX_SIG_UNBLOCK) {
    how = SIG_UNBLOCK;
  }

  if (how == HPX_SIG_SET) {
    how = SIG_SETMASK;
  }

  dbg_check(pthread_sigmask(how, &set, &set));

  if (sigismember(&set, SIGSEGV)) {
    mask |= HPX_SIGSEGV;
  }

  if (sigismember(&set, SIGABRT)) {
    mask |= HPX_SIGABRT;
  }

  if (sigismember(&set, SIGFPE)) {
    mask |= HPX_SIGFPE;
  }

  if (sigismember(&set, SIGILL)) {
    mask |= HPX_SIGILL;
  }

  if (sigismember(&set, SIGBUS)) {
    mask |= HPX_SIGBUS;
  }

  if (sigismember(&set, SIGIOT)) {
    mask |= HPX_SIGIOT;
  }

  if (sigismember(&set, SIGSYS)) {
    mask |= HPX_SIGSYS;
  }

  if (sigismember(&set, SIGTRAP)) {
    mask |= HPX_SIGTRAP;
  }

  return mask;
}

#endif
