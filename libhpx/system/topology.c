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

#include <alloca.h>
#include <string.h>
#include <unistd.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/system.h>
#include <libhpx/topology.h>
#include <libhpx/utils.h>
#include <hwloc.h>

static int _get_resources_by_affinity(topology_t *topology,
                                      libhpx_thread_affinity_t policy) {
  int n = 0;
  switch (policy) {
   case HPX_THREAD_AFFINITY_DEFAULT:
   case HPX_THREAD_AFFINITY_NUMA:
     n = topology->nnodes;
     break;
   case HPX_THREAD_AFFINITY_CORE:
     n = topology->ncores;
     break;
   case HPX_THREAD_AFFINITY_HWTHREAD:
     n = topology->ncpus;
     break;
   case HPX_THREAD_AFFINITY_NONE:
     return 0;
   default:
     log_error("unknown thread affinity policy\n");
     return 0;
  }
  return n;
}

static hwloc_cpuset_t *_cpu_affinity_map_new(topology_t *topology,
                                             libhpx_thread_affinity_t policy) {
  int resources = _get_resources_by_affinity(topology, policy);
  if (!resources) {
    return NULL;
  }
  hwloc_cpuset_t *cpu_affinity_map = calloc(resources, sizeof(*cpu_affinity_map));

  for (int r = 0; r < resources; ++r) {
    hwloc_cpuset_t cpuset = cpu_affinity_map[r] = hwloc_bitmap_alloc();
    switch (policy) {
     case HPX_THREAD_AFFINITY_DEFAULT:
     case HPX_THREAD_AFFINITY_NUMA:
       for (int i = 0; i < topology->ncpus; ++i) {
         if (r == topology->cpu_to_numa[i]) {
           hwloc_bitmap_set(cpuset, i);
         }
       }
       break;
     case HPX_THREAD_AFFINITY_CORE:
       for (int i = 0; i < topology->ncpus; ++i) {
         if (r == topology->cpu_to_core[i]) {
           hwloc_bitmap_set(cpuset, i);
         }
       }
       break;
     case HPX_THREAD_AFFINITY_HWTHREAD:
       hwloc_bitmap_set(cpuset, r);
       hwloc_bitmap_singlify(cpuset);
       break;
     case HPX_THREAD_AFFINITY_NONE:
       hwloc_bitmap_set_range(cpuset, 0, topology->ncpus-1);
       break;
     default:
       log_error("unknown thread affinity policy\n");
       return NULL;
    }
  }
  return cpu_affinity_map;
}

static void _cpu_affinity_map_delete(topology_t *topology) {
  int resources = _get_resources_by_affinity(topology, here->config->thread_affinity);
  for (int r = 0; r < resources; ++r) {
    hwloc_bitmap_free(topology->cpu_affinity_map[r]);
  }
  free(topology->cpu_affinity_map);
}

topology_t *topology_new(const struct config *config) {
  // Allocate the topology structure/
  topology_t *topo = malloc(sizeof(*topo));
  if (!topo) {
    log_error("failed to allocate topology structure\n");
    return NULL;
  }

  // Provide initial values.
  topo->ncpus = 0;
  topo->ncores = 0;
  topo->nnodes = 0;
  topo->numa_nodes = NULL;
  topo->cpus_per_node = 0;
  topo->cpu_to_core = 0;
  topo->cpu_to_numa = NULL;
  topo->numa_to_cpus = 0;
  topo->cpu_affinity_map = NULL;

  // Initialize the hwloc infrastructure.
  hwloc_topology_t *hwloc = &topo->hwloc_topology;
  if (hwloc_topology_init(hwloc)) {
    log_error("failed to initialize HWLOC topology.\n");
    topology_delete(topo);
    return NULL;
  }

  if (hwloc_topology_load(*hwloc)) {
    log_error("failed to load HWLOC topology.\n");
    topology_delete(topo);
    return NULL;
  }

  // Query the hwloc object for cpus, cores, and numa nodes---"fix" nnodes if
  // hwloc returns something unpleasant
  topo->ncpus = hwloc_get_nbobjs_by_type(*hwloc, HWLOC_OBJ_PU);
  topo->ncores = hwloc_get_nbobjs_by_type(*hwloc, HWLOC_OBJ_CORE);
  topo->nnodes = hwloc_get_nbobjs_by_type(*hwloc, HWLOC_OBJ_NODE);
  topo->nnodes = (topo->nnodes > 0) ? topo->nnodes : 1;
  topo->cpus_per_node = topo->ncpus / topo->nnodes;

  // Allocate our secondary arrays.
  topo->allowed_cpus = hwloc_bitmap_alloc();
  if (!topo->allowed_cpus) {
    log_error("failed to allocate memory for cpuset.\n");
    topology_delete(topo);
    return NULL;
  }

  topo->cpus = calloc(topo->ncpus, sizeof(topo->cpus[0]));
  if (!topo->cpus) {
    log_error("failed to allocate memory for cpu objects.\n");
    topology_delete(topo);
    return NULL;
  }

  topo->cpu_to_core = calloc(topo->ncpus, sizeof(topo->cpu_to_core[0]));
  if (!topo->cpu_to_core) {
    log_error("failed to allocate memory for the core map.\n");
    topology_delete(topo);
    return NULL;
  }

  topo->cpu_to_numa = calloc(topo->ncpus, sizeof(topo->cpu_to_numa[0]));
  if (!topo->cpu_to_numa) {
    log_error("failed to allocate memory for the NUMA map.\n");
    topology_delete(topo);
    return NULL;
  }

  topo->numa_nodes = calloc(topo->nnodes, sizeof(topo->numa_nodes[0]));
  if (!topo->numa_nodes) {
    log_error("failed to allocate memory for numa node objects.\n");
    topology_delete(topo);
    return NULL;
  }

  topo->numa_to_cpus = calloc(topo->nnodes, sizeof(topo->numa_to_cpus[0]));
  if (!topo->numa_to_cpus) {
    log_error("failed to allocate memory for the reverse NUMA map.\n");
    topology_delete(topo);
    return NULL;
  }

  for (int i = 0, e = topo->nnodes; i < e; ++i) {
    topo->numa_to_cpus[i] = calloc(topo->cpus_per_node, sizeof(int));
    if (!topo->numa_to_cpus[i]) {
      log_error("failed to allocate memory for the reverse NUMA map.\n");
      topology_delete(topo);
      return NULL;
    }
  }

  // Detect how many CPUs we can run on based on affinity or environment.  On
  // aprun platforms, we look at the ALPS depth to figure out how many CPUs to
  // use, otherwise we try and use hwloc to figure out what the process mask is
  // set to, otherwise we just bail out and use all CPUs.
  int cores = libhpx_getenv_num("ALPS_APP_DEPTH", 0);
  if (cores) {
    hwloc_bitmap_set_range(topo->allowed_cpus, 0, cores - 1);
  }
  else if (hwloc_get_cpubind(*hwloc, topo->allowed_cpus, HWLOC_CPUBIND_PROCESS))
  {
    hwloc_bitmap_set_range(topo->allowed_cpus, 0, topo->ncpus - 1);
  }

  // We want to be able to map from a numa domain to its associated cpus, but we
  // are going to iterate through the cpus in an undefined order. This array
  // keeps track of the "next" index for each numa node array, so that we can
  // insert into the right place.
  int numa_to_cpus_next[topo->nnodes];
  memset(numa_to_cpus_next, 0, sizeof(numa_to_cpus_next));

  hwloc_obj_t cpu = NULL;
  hwloc_obj_t core = NULL;
  hwloc_obj_t numa_node = NULL;
  for (int i = 0, e = topo->ncpus; i < e; ++i) {
    // get the hwloc tree nodes for the cpu, core, and numa node
    cpu = hwloc_get_next_obj_by_type(*hwloc, HWLOC_OBJ_PU, cpu);
    core = hwloc_get_ancestor_obj_by_type(*hwloc, HWLOC_OBJ_CORE, cpu);
    numa_node = hwloc_get_ancestor_obj_by_type(*hwloc, HWLOC_OBJ_NODE, cpu);

    // get integer indexes for the cpu, core, and numa node
    int index = cpu->os_index;
    int core_index = (core) ? core->os_index : 0;
    int numa_index = (numa_node) ? numa_node->os_index : 0;

    // record our hwloc nodes so that we can get them quickly during queries
    topo->cpus[index] = cpu;
    topo->numa_nodes[numa_index] = numa_node;

    // record our core and numa indices for quick query
    topo->cpu_to_core[index] = core_index;
    topo->cpu_to_numa[index] = numa_index;

    // and record this cpu in the correct numa set
    int next_cpu_index = numa_to_cpus_next[numa_index]++;
    topo->numa_to_cpus[numa_index][next_cpu_index] = index;
  }

  // generate the CPU affinity map
  topo->cpu_affinity_map = _cpu_affinity_map_new(topo, config->thread_affinity);
  return topo;
}

void topology_delete(topology_t *topology) {
  if (!topology) {
    return;
  }

  if (topology->cpus) {
    free(topology->cpus);
    topology->cpus = NULL;
  }

  if (topology->numa_nodes) {
    free(topology->numa_nodes);
    topology->numa_nodes = NULL;
  }

  if (topology->cpu_to_core) {
    free(topology->cpu_to_core);
    topology->cpu_to_core = NULL;
  }

  if (topology->cpu_to_numa) {
    free(topology->cpu_to_numa);
    topology->cpu_to_numa = NULL;
  }

  if (topology->numa_to_cpus) {
    for (int i = 0; i < topology->nnodes; ++i) {
      if (topology->numa_to_cpus[i]) {
        free(topology->numa_to_cpus[i]);
      }
    }
    free(topology->numa_to_cpus);
    topology->numa_to_cpus = NULL;
  }

  if (topology->allowed_cpus) {
    hwloc_bitmap_free(topology->allowed_cpus);
    topology->allowed_cpus = NULL;
  }

  if (topology->cpu_affinity_map) {
    _cpu_affinity_map_delete(topology);
    topology->cpu_affinity_map  = NULL;
  }

  hwloc_topology_destroy(topology->hwloc_topology);
  free(topology);
}
