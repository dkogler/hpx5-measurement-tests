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

#ifndef LIBHPX_NETWORK_PWC_PARCEL_EMULATION_H
#define LIBHPX_NETWORK_PWC_PARCEL_EMULATION_H

struct boot;
struct config;
struct pwc_xport;

typedef struct parcel_emulator {
  void (*delete)(void *obj);
  int (*send)(void *obj, struct pwc_xport *xport, int rank,
              const hpx_parcel_t *p);
  hpx_parcel_t *(*recv)(void *obj, int rank);
} parcel_emulator_t;

void *parcel_emulator_new_reload(const struct config *cfg, struct boot *boot,
                                 struct pwc_xport *xport);

static inline void parcel_emulator_delete(void *obj) {
  parcel_emulator_t *emulator = obj;
  emulator->delete(obj);
}

static inline int parcel_emulator_send(void *obj, struct pwc_xport *xport,
                                       int rank, const hpx_parcel_t *p) {
  parcel_emulator_t *emulator = obj;
  return emulator->send(obj, xport, rank, p);
}

static inline hpx_parcel_t *parcel_emulator_recv(void *obj, int rank) {
  parcel_emulator_t *emulator = obj;
  return emulator->recv(obj, rank);
}

#endif
