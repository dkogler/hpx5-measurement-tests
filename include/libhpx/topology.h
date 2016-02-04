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

#ifndef LIBHPX_TOPOLOGY_H
#define LIBHPX_TOPOLOGY_H


/// @file include/libhpx/topology.h
#include <stdint.h>
#include <hpx/hpx.h>
#include <hwloc.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Forward declarations
/// @{
struct config;
/// @}

/// @struct topology
/// @brief The "physical" topology object.
///
/// @var topology::hwloc_topology
/// The HWLOC topology object.
/// @var topology::ncpus
/// The number of physical CPUs in the system.
/// @var topology::cpus
/// The HWLOC object corresponding to each CPU.
/// @var topology::ncores
/// The number of physical cores in the system.
/// @var topology::nnodes
/// The number of NUMA nodes in the system.
/// @var topology::numa_nodes
/// The HWLOC object corresponding to each NUMA node.
/// @var topology::cpus_per_node
/// The number of CPUs per NUMA node
/// @var topology::cpu_to_core
/// The CPU to core mapping.
/// @var topology::cpu_to_numa
/// The CPU to NUMA node mapping.
/// @var topology::numa_to_cpus
/// The (reverse) NUMA node to cpus mapping.
/// @var topology::allowed_cpus
/// The initial CPU binding of the HPX process.
/// @var topology::cpu_affinity_map
/// The CPU affinity map that maintains the CPU binding for a resource
/// (numa-node, core-id) depending on the global affinity policy.
typedef struct topology {
  hwloc_topology_t  hwloc_topology;
  int                        ncpus;
  hwloc_obj_t                *cpus;
  int                       ncores;
  int                       nnodes;
  hwloc_obj_t          *numa_nodes;
  int                cpus_per_node;
  int                 *cpu_to_core;
  int                 *cpu_to_numa;
  int               **numa_to_cpus;
  hwloc_cpuset_t      allowed_cpus;
  hwloc_cpuset_t *cpu_affinity_map;
} topology_t;

/// Allocate and initialize a new topology object.
///
/// @param       config The configuration object.
///
/// @returns            The topology object, or NULL if there was an error.
topology_t *topology_new(const struct config *config)
  HPX_MALLOC;

/// Finalize and free the topology object.
///
/// @param    topology The topology object to free.
void topology_delete(topology_t *topology);

// if hpx_addr_t and uint64_t do not match, this header will need rewritten
_HPX_ASSERT(sizeof(hpx_addr_t) == sizeof(uint64_t), hpx_addr_t_size);

// the number of bits for each part of the packed value
#define     TOPO_PE_BITS (16)
#define TOPO_WORKER_BITS (8)
#define TOPO_OFFSET_BITS (8 * sizeof(uint64_t) - TOPO_PE_BITS \
                          - TOPO_WORKER_BITS)

// shift values for the three parts of the packed value
#define     TOPO_PE_SHIFT (TOPO_WORKER_BITS + TOPO_OFFSET_BITS)
#define TOPO_WORKER_SHIFT (TOPO_OFFSET_BITS)
#define TOPO_OFFSET_SHIFT (0)

// masks to clobber bits of the address (use with &)
#define TOPO_LOCATION_MASK (UINT64_MAX << TOPO_WORKER_SHIFT)
#define       TOPO_PE_MASK (UINT64_MAX << TOPO_PE_SHIFT)
#define   TOPO_WORKER_MASK ((UINT64_MAX << TOPO_WORKER_SHIFT) & \
                            (~TOPO_PE_MASK))
#define   TOPO_OFFSET_MASK (~(TOPO_LOCATION_MASK))
#define  TOPO_MAX_LG_BSIZE (sizeof(uint32_t)*8)

/// Extract the locality from a packed value.
static inline uint32_t topo_value_to_rank(uint64_t value) {
  return (value & TOPO_PE_MASK) >> TOPO_PE_SHIFT;
}

/// Extract the locality from a packed value.
static inline uint32_t topo_value_to_worker(uint64_t value) {
  return (value & TOPO_WORKER_MASK) >> TOPO_WORKER_SHIFT;
}

/// Extract the offset value of a packed value, given the number of ranks.
/// @param        value The tological, packed value
///
/// @returns            The offset part of @p value
static inline uint64_t topo_value_to_offset(uint64_t value) {
  return (value & TOPO_OFFSET_MASK) >> TOPO_OFFSET_SHIFT;
}

/// Create a topological, packed value from a locality and offset value
///
/// @param     locality The locality to be packed with this value
/// @param       worker The worker id to be packed with this value
/// @param         offset The offset value to be packed with this packed value
///
/// @returns            A packed value encoding the locality and the offset
static inline uint64_t topo_offset_to_value(uint32_t locality, uint32_t worker,
                                          uint64_t offset)
{
  uint64_t pe = (((uint64_t)locality) << TOPO_PE_SHIFT) & TOPO_PE_MASK;
  uint64_t core = (((uint64_t)worker) << TOPO_WORKER_SHIFT) & TOPO_WORKER_MASK;
  uint64_t dat = (offset << TOPO_OFFSET_SHIFT) & TOPO_OFFSET_MASK;
  return pe + core + dat;
}

#ifdef __cplusplus
}
#endif

#endif // LIBHPX_TOPOLOGY_H
