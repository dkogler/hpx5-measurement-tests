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

#include <limits.h>
#include <stdint.h>

#include <libhpx/instrumentation.h>
#include "metadata.h"

// Possibly, we might want to move more macros from the header into here as
// variables

const inst_event_metadata_t INST_EVENT_METADATA[TRACE_NUM_EVENTS] =
{
#define LIBHPX_EVENT(class, event, ...) class##_##event##_METADATA,
# include <libhpx/events.def>
#undef LIBHPX_EVENT
};
