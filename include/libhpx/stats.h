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

#ifndef LIBHPX_STATS_H
#define LIBHPX_STATS_H

#include "hpx/hpx.h"

/// @file libhpx/scheduler/stats.h
/// @brief The libhpx stats definitions.

#if defined(ENABLE_PROFILING) || defined(HAVE_APEX)
#define COUNTER_SAMPLE(e) e
#else
#define COUNTER_SAMPLE(e)
#endif

/// libhpx statistics.
///
/// These are statistics mostly related to the scheduler that are
/// updated during program execution. Local (per-worker) statistics
/// are stored in each worker's TLS and updated by each worker
/// separately. Global (per-scheduler) statistics are accumulated
/// after each worker shuts down.
typedef struct libhpx_stats {
  unsigned long        spawns;
  unsigned long failed_steals;
  unsigned long        steals;
  unsigned long          mail;
  unsigned long        stacks;
  unsigned long        yields;
} libhpx_stats_t;

#define LIBHPX_STATS_INIT { \
    .spawns        = 0,     \
    .failed_steals = 0,     \
    .steals        = 0,     \
    .mail          = 0,     \
    .stacks        = 0,     \
    .yields        = 0,     \
  }

/// Initialize the libhpx statistics structure.
void libhpx_stats_init(struct libhpx_stats *stats)
  HPX_NON_NULL(1);

/// Accumulate per-worker libhpx statistics.
///
/// This is not synchronized.
///
/// @param          lhs The counts we're accumulating into.
/// @param          rhs The counts we're trying to add to.
///
/// @returns            lhs
struct libhpx_stats *libhpx_stats_accum(struct libhpx_stats *lhs,
                                        const struct libhpx_stats *rhs)
  HPX_NON_NULL(1, 2);

/// Print libhpx statistics.
void libhpx_stats_print(void);

/// Save collected statistics to APEX.
void libhpx_save_apex_stats(void);

#endif // LIBHPX_STATS_H
