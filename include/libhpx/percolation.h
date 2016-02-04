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

#ifndef LIBHPX_PERCOLATION_H
#define LIBHPX_PERCOLATION_H

/// @file include/libhpx/percolation.h
/// @brief Exports methods and structures related to percolation.
///

#ifdef __cplusplus
extern "C" {
#endif

#include "hpx/attributes.h"
#include "hpx/hpx.h"
#include "libhpx/config.h"

typedef struct {
  const char *(*id)(void);
  void (*delete)(void*);
  void *(*prepare)(const void*, const char *, const char *);
  int (*execute)(const void*, void *, int, void **, size_t *);
  void (*destroy)(const void*, void *);
} percolation_t;

percolation_t *percolation_new_opencl(void);
percolation_t *percolation_new(void);

/// Percolation  interface.
/// @{

/// Delete a percolation object.
static inline void percolation_delete(void *obj) {
  percolation_t *percolation = obj;
  percolation->delete(percolation);
}

/// Percolate an action to be executed later on a device.
static inline
void *percolation_prepare(const void *obj, const char *key,
                          const char *kernel) {
  const percolation_t *percolation = obj;
  return percolation->prepare(percolation, key, kernel);
}

/// Execute a percolation object on an available device.
static inline
int percolation_execute(const void *obj, void *o, int n, void *vargs[],
                        size_t sizes[]) {
  const percolation_t *percolation = obj;
  return percolation->execute(percolation, o, n, vargs, sizes);
}

/// Destroy or release the state associated with a percolation object.
static inline void percolation_destroy(const void *obj, void *o) {
  const percolation_t *percolation = obj;
  percolation->destroy(percolation, o);
}

/// An action to launch OpenCL kernels.
extern HPX_ACTION_DECL(percolation_execute_action);

/// The action handler that launches OpenCL kernels.
int percolation_execute_handler(int nargs, void *vargs[],
        size_t sizes[]);
/// @}


#ifdef __cplusplus
}
#endif

#endif // LIBHPX_PERCOLATION_H
