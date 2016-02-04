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

#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <hpx/hpx.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/percolation.h>

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

/// Derived OpenCL percolation class
typedef struct {
  const char *(*id)(void);
  void (*delete)(void*);
  void *(*prepare)(const void*, const char *, const char *);
  int (*execute)(const void*, void *, int, void **, size_t *);
  void (*destroy)(const void*, void *);

  cl_device_id device;
  cl_platform_id platform;
  cl_context context;
  cl_command_queue queue;
} _opencl_percolation_t;

static HPX_RETURNS_NON_NULL const char *_id(void) {
  return "OPENCL";
}

static void _delete(void *percolation) {
  _opencl_percolation_t *cl = percolation;
  clReleaseCommandQueue(cl->queue);
  clReleaseContext(cl->context);
}

static void *_prepare(const void *percolation, const char *key,
                      const char *kernel) {
  _opencl_percolation_t *cl = percolation;

  int e;
  cl_program program = clCreateProgramWithSource(cl->context, 1, &kernel,
                                                 NULL, &e);
  dbg_assert_str(e >= 0, "failed to create an OpenCL program for %s.\n", key);

  e = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
  dbg_assert_str(e >= 0, "failed to build OpenCL program for %s.\n", key);

  char *name = strchr(key, ':')+1;
  dbg_assert(name);
  cl_kernel k = clCreateKernel(program, name, &e);
  dbg_assert_str(e >= 0, "failed to create OpenCL kernel for %s.\n", key);

  return (void*)k;
}

static int _execute(const void *percolation, void *obj, int nargs,
                    void *vargs[], size_t sizes[]) {
  _opencl_percolation_t *cl = percolation;
  cl_kernel kernel = (cl_kernel)obj;

  cl_mem buf[nargs];
  int e;
  for (int i = 0; i < nargs; ++i) {
    buf[i] = clCreateBuffer(cl->context,
                            CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
                            sizes[i], vargs[i], &e);
    dbg_assert_str(e >= 0, "failed to create OpenCL input buffer.\n");

    e = clSetKernelArg(kernel, i*2, sizeof(cl_mem), &buf[i]);
    dbg_assert_str(e >= 0, "failed to set OpenCL kernel input arg.\n");

    e = clSetKernelArg(kernel, (i*2)+1, sizeof(size_t), &sizes[i]);
    dbg_assert_str(e >= 0, "failed to set OpenCL kernel input arg.\n");
  }

  // TODO: support arbitrarily-sized outputs
  size_t osize = sizes[0];
  void *output = calloc(1, osize);
  dbg_assert(output);

  cl_mem obuf = clCreateBuffer(cl->context, CL_MEM_WRITE_ONLY,
                               osize, NULL, &e);
  dbg_assert_str(e >= 0, "failed to create OpenCL output buffer.\n");

  e = clSetKernelArg(kernel, nargs, sizeof(cl_mem), &obuf);
  dbg_assert_str(e >= 0, "failed to set OpenCL kernel output arg.\n");

  e = clSetKernelArg(kernel, nargs+1, sizeof(size_t), &osize);
  dbg_assert_str(e >= 0, "failed to set OpenCL kernel output arg.\n");

  // TODO: determine optimal work group size.
  // TODO: look at clEnqueueTask.
  size_t local_size = 64;
  size_t global_size = ((osize+local_size)/local_size)*local_size;

  e = clEnqueueNDRangeKernel(cl->queue, kernel, 1, NULL, &global_size,
                             &local_size, 0, NULL, NULL);

  // TODO: make kernel execution asynchronous
  clFinish(cl->queue);

  clEnqueueReadBuffer(cl->queue, obuf, CL_TRUE, 0, osize, output,
                      0, NULL, NULL);

  for (int i = 0; i < nargs; ++i) {
    clReleaseMemObject(buf[i]);
  }

  int e = hpx_thread_continue(output, osize);

  clReleaseMemObject(obuf);
  if (output) {
    free(output);
  }

  return e;
}

static void _destroy(const void *percolation, void *obj) {
  cl_kernel kernel = (cl_kernel)obj;
  clReleaseKernel(kernel);
}

static _opencl_percolation_t _opencl_percolation_class = {
  .id        = _id,
  .delete    = _delete,
  .prepare   = _prepare,
  .execute   = _execute,
  .destroy   = _destroy,
  .device    = NULL,
  .platform  = NULL,
  .context   = NULL,
  .queue     = NULL
};

/// Create a new percolation object.
///
/// This functions discovers the available resources in the system. If a
/// GPU is not found, it falls back to using a CPU. (We may or may not
/// want to do this depending on how many worker threads we have
/// spawned). Further, it initializes all of the structures necessary
/// to execute jobs on this resource.
percolation_t *percolation_new_opencl(void) {
  _opencl_percolation_t *class = &_opencl_percolation_class;

  int e = clGetPlatformIDs(1, &class->platform, NULL);
  dbg_assert_str(e >= 0, "failed to identify the OpenCL platform.\n");

  e = clGetDeviceIDs(class->platform, CL_DEVICE_TYPE_GPU, 1,
                     &class->device, NULL);
  if (e == CL_DEVICE_NOT_FOUND) {
    log_dflt("GPU device not found. Using the CPU...\n");
    e = clGetDeviceIDs(class->platform, CL_DEVICE_TYPE_CPU, 1,
                       &class->device, NULL);
  }
  dbg_assert_str(e >= 0, "failed to access the OpenCL device.\n");

  class->context = clCreateContext(NULL, 1, &class->device, NULL, NULL, &e);
  dbg_assert_str(e >= 0, "failed to create an OpenCL context.\n");

  class->queue = clCreateCommandQueue(class->context, class->device, 0, &e);
  dbg_assert_str(e >= 0, "failed to create an OpenCL command queue.\n");

  return (percolation_t*)class;
}
