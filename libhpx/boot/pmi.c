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
#include <config.h>
#endif

#include <stdlib.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>
#include <pmi.h>

#include <libhpx/boot.h>
#include <libhpx/locality.h>
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>


static HPX_RETURNS_NON_NULL const char *_id(void) {
  return "PMI";
}


static void _delete(boot_t *boot) {
  PMI_Finalize();
}


static int _rank(const boot_t *boot) {
  int rank;
  if (PMI_Get_rank(&rank) != PMI_SUCCESS)
    hpx_abort();
  return rank;
}


static int _n_ranks(const boot_t *boot) {
  int ranks;
  if (PMI_Get_size(&ranks) != PMI_SUCCESS)
    hpx_abort();
  return ranks;
}


static int _barrier(const boot_t *boot) {
  return (PMI_Barrier() != PMI_SUCCESS) ? LIBHPX_ERROR : LIBHPX_OK;
}


static void _abort(const boot_t *boot) {
  PMI_Abort(-6, "HPX aborted.");
}


/// ----------------------------------------------------------------------------
/// Base64 string encoding.
///
/// todo: Cray PMI does not allow characters such as /;=. We need to
/// make the encoding function account for that.
/// ----------------------------------------------------------------------------
static int
_encode(const void *src, size_t slen, char *dst, size_t *dlen) {
  assert ((dlen != NULL) || (dst != NULL));
  unsigned char *s = (unsigned char*)src;
  char *d = (char*)dst;
  char *p;

  *dlen = ((slen+3)/4)*6+1;
  uint64_t w;
  while (slen > 3) {
    w = *s++; w = (w << 8) | *s++; w = (w << 8) | *s++; w = (w << 8) | *s++;
    slen -= 4;
    p = stpcpy(d, l64a(htonl(w)));
    d = mempcpy(p, "......", 6-(p-d));
  }

  if (slen > 0) {
    w = *s++;
    if (--slen > 0) {
      w = (w << 8) | *s++;
      if (--slen > 0)
        w = (w << 8) | *s;
    }
    d = stpcpy(d, l64a(htonl(w)));
  }
  *d = '\0';
  return LIBHPX_OK;
}

/// ----------------------------------------------------------------------------
/// Base64 string decoding. The length of source and destination
/// buffers is always unequal due to padding.
/// ----------------------------------------------------------------------------
static int
_decode(const char *src, size_t slen, void *dst, size_t dlen) {
  assert (dst != NULL);
  char *s = (char*)src;
  unsigned char *d = (unsigned char*)dst;

  assert(dlen >= (2*slen-11)/3);
  long w;
  for (int i = 0, j = 0; i < slen; j+=4, i+=6) {
    w = ntohl(a64l(s+i));
    d[j+3] = w & 0xFF;
    w = (w >> 8);
    d[j+2] = w & 0xFF;
    w = (w >> 8);
    d[j+1] = w & 0xFF;
    w = (w >> 8);
    d[j] = w & 0xFF;
  }
  // handle left-over?

  return LIBHPX_OK;
}

/// ----------------------------------------------------------------------------
/// This publishes a buffer in PMI's key-value store. The buffer
/// is associated with our rank as its key. Since the value needs to
/// be a string buffer, it is first encoded using base64 encoding
/// before doing a KVS put.
/// ----------------------------------------------------------------------------
static int HPX_USED _put_buffer(char *kvs, int rank, void *buffer, size_t len)
{
  int length;
  const int pmi_maxlen = 256;

  // allocate key
  int e = PMI_KVS_Get_key_length_max(&length);
  if (e != PMI_SUCCESS) {
    log_error("pmi: failed to get max key length.\n");
    length = pmi_maxlen;
  }
  char *key = malloc(sizeof(*key) * length);
  snprintf(key, length, "%d", rank);

  // allocate value
  e = PMI_KVS_Get_value_length_max(&length);
  if (e != PMI_SUCCESS) {
    free(key);
    log_error("pmi: failed to get max value length.\n");
    length = pmi_maxlen;
  }
  char *value = malloc(sizeof(*value) * length);
  if ((_encode(buffer, len, value, (size_t*)&length)) != LIBHPX_OK) {
    goto error;
  }

  // do the key-value pair exchange
  if ((PMI_KVS_Put(kvs, key, value)) != PMI_SUCCESS) {
    goto error;
  }

  if ((PMI_KVS_Commit(kvs)) != PMI_SUCCESS) {
    goto error;
  }

  PMI_Barrier();
  free(key);
  free(value);
  return length;

error:
  free(key);
  free(value);
  return log_error("pmi: failed to put buffer in PMI's KVS.\n");
}

/// ----------------------------------------------------------------------------
/// This retrieves a buffer from PMI's key-value store. The buffer is
/// base64 decoded before it is returned.
/// ----------------------------------------------------------------------------
static int HPX_USED _get_buffer(char *kvs, int rank, void *buffer, size_t len)
{
  int length;

  // allocate key
  int e = PMI_KVS_Get_key_length_max(&length);
  if (e != PMI_SUCCESS)
    return log_error("pmi: failed to get max key length.\n");
  char *key = malloc(sizeof(*key) * length);
  snprintf(key, length, "%d", rank);

  // allocate value
  e = PMI_KVS_Get_value_length_max(&length);
  if (e != PMI_SUCCESS) {
    free(key);
    return log_error("pmi: failed to get max value length.\n");
  }
  char *value = malloc(sizeof(*value) * length);
  if ((PMI_KVS_Get(kvs, key, value, length)) != PMI_SUCCESS)
    goto error;

  if ((_decode(value, strlen(value), buffer, len)) != LIBHPX_OK)
    goto error;

  free(key);
  free(value);
  return LIBHPX_OK;

error:
  free(key);
  free(value);
  return log_error("pmi: failed to put buffer in PMI's KVS.\n");
}


static int _allgather(const boot_t *boot, const void *restrict cin,
                      void *restrict out, int n) {
  int rank = here->rank;
  int nranks = here->ranks;
  void *in = (void*)cin;

#if HAVE_PMI_CRAY_EXT
  // nb: Cray PMI allgather does not guarantee process-rank
  // order. Here, we assume that the ordering is, at least,
  // deterministic for all allgather invocations.

  int *ranks = malloc(sizeof(rank) * nranks);
  if ((PMI_Allgather(&rank, ranks, sizeof(rank))) != PMI_SUCCESS) {
    free(ranks);
    return log_error("pmi: failed in PMI_Allgather.\n");
  }
  
  void *buf = malloc(sizeof(char) * n * nranks);
  assert(buf != NULL);
  if ((PMI_Allgather(in, buf, n)) != PMI_SUCCESS) {
    free(buf);
    free(ranks);
    return log_error("pmi: failed in PMI_Allgather.\n");
  }

  for (int i = 0; i < nranks; i++) {
    memcpy((char*)out+(ranks[i]*n), (char*)buf+(i*n), n);
  }

  free(buf);
  free(ranks);
#else
  int length;

  // allocate name for the nidpid map exchange
  int e = PMI_KVS_Get_name_length_max(&length);
  if (e != PMI_SUCCESS) {
    return log_error("pmi: failed to get max name length.\n");
  }
  char *name = malloc(sizeof(*name) * length);
  e = PMI_KVS_Get_my_name(name, length);
  if (e != PMI_SUCCESS) {
    free(name);
    return log_error("pmi: failed to get kvs name.\n");
  }

  _put_buffer(name, nranks, (void*)in, n);

  for (int r = 0; r < nranks; r++)
    _get_buffer(name, r, (char*)out+(r*n), n);

#endif
  return LIBHPX_OK;
}

static int _pmi_alltoall(const void *boot, void *restrict dest,
                         const void *restrict src, int n, int stride) {
  // Emulate alltoall with allgather for now.
  const boot_t *pmi = boot;
  int rank = pmi->rank(pmi);
  int nranks = pmi->n_ranks(pmi);

  // Allocate a temporary buffer to allgather into
  int gather_bytes = nranks * nranks * stride;
  void *gather = malloc(gather_bytes);
  if (!gather) {
    dbg_error("could not allocate enough space for PMI alltoall emulation\n");
  }
  
  // Perform the allgather
  int e = _allgather(pmi, src, gather, nranks * stride);
  if (LIBHPX_OK != e) {
    dbg_error("could not gather in PMI alltoall emulation\n");
  }

  // Copy out the data
  const char *from = gather;
  char *to = dest;
  for (int i = 0, e = nranks; i < e; ++i) {
    int offset = (i * nranks * stride) + (rank * stride);
    memcpy(to + (i * stride), from + offset, n);
  }

  free(gather);
  return LIBHPX_OK;
}

static boot_t _pmi = {
  .type      = HPX_BOOT_PMI,
  .id        = _id,
  .delete    = _delete,
  .rank      = _rank,
  .n_ranks   = _n_ranks,
  .barrier   = _barrier,
  .allgather = _allgather,
  .alltoall  = _pmi_alltoall,
  .abort     = _abort
};

boot_t *boot_new_pmi(void) {
  int init;
  PMI_Initialized(&init);
  if (init) {
    return &_pmi;
  }

  int spawned;
  if (PMI_Init(&spawned) == PMI_SUCCESS) {
    log_boot("initialized PMI boostrapper.\n");
    return &_pmi;
  }

  log_boot("failed to initialize PMI bootstrap network\n");
  return NULL;
}
