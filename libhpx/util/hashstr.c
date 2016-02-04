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

/// @file libhpx/utils/hashstr.c
/// @brief Implements string hashing.

#include <stdint.h>
#include <string.h>
#include <stdio.h>


/// Bob Jenkin's spooky hash function (Public Domain)
/// http://burtleburtle.net/bob/hash/spooky.html

static inline uint64_t
rot64(uint64_t x, int k) {
  return (x << k) | (x >> (64 - k));
}


static inline void
_mix(const uint64_t *data, uint64_t *s0, uint64_t *s1, uint64_t *s2,
     uint64_t *s3, uint64_t *s4, uint64_t *s5, uint64_t *s6, uint64_t *s7,
     uint64_t *s8, uint64_t *s9, uint64_t *s10, uint64_t *s11) {
  *s0 += data[0];   *s2 ^= *s10; *s11 ^= *s0; *s0 = rot64(*s0, 11);  *s11 += *s1;
  *s1 += data[1];   *s3 ^= *s11; *s0 ^= *s1;  *s1 = rot64(*s1, 32);  *s0 += *s2;
  *s2 += data[2];   *s4 ^= *s0;  *s1 ^= *s2;  *s2 = rot64(*s2, 43);  *s1 += *s3;
  *s3 += data[3];   *s5 ^= *s1;  *s2 ^= *s3;  *s3 = rot64(*s3, 31);  *s2 += *s4;
  *s4 += data[4];   *s6 ^= *s2;  *s3 ^= *s4;  *s4 = rot64(*s4, 17);  *s3 += *s5;
  *s5 += data[5];   *s7 ^= *s3;  *s4 ^= *s5;  *s5 = rot64(*s5, 28);  *s4 += *s6;
  *s6 += data[6];   *s8 ^= *s4;  *s5 ^= *s6;  *s6 = rot64(*s6, 39);  *s5 += *s7;
  *s7 += data[7];   *s9 ^= *s5;  *s6 ^= *s7;  *s7 = rot64(*s7, 57);  *s6 += *s8;
  *s8 += data[8];   *s10 ^= *s6; *s7 ^= *s8;  *s8 = rot64(*s8, 55);  *s7 += *s9;
  *s9 += data[9];   *s11 ^= *s7; *s8 ^= *s9;  *s9 = rot64(*s9, 54);  *s8 += *s10;
  *s10 += data[10]; *s0 ^= *s8; *s9 ^= *s10;  *s10 = rot64(*s10, 22); *s9 += *s11;
  *s11 += data[11]; *s1 ^= *s9; *s10 ^= *s11; *s11 = rot64(*s11, 46); *s10 += *s0;
}


static inline void
_end_partial(uint64_t *h0, uint64_t *h1, uint64_t *h2,  uint64_t *h3,
             uint64_t *h4, uint64_t *h5, uint64_t *h6,  uint64_t *h7,
             uint64_t *h8, uint64_t *h9, uint64_t *h10, uint64_t *h11) {
  *h11+= *h1;   *h2 ^= *h11;  *h1 = rot64(*h1, 44);
  *h0 += *h2;   *h3 ^= *h0;   *h2 = rot64(*h2, 15);
  *h1 += *h3;   *h4 ^= *h1;   *h3 = rot64(*h3, 34);
  *h2 += *h4;   *h5 ^= *h2;   *h4 = rot64(*h4, 21);
  *h3 += *h5;   *h6 ^= *h3;   *h5 = rot64(*h5, 38);
  *h4 += *h6;   *h7 ^= *h4;   *h6 = rot64(*h6, 33);
  *h5 += *h7;   *h8 ^= *h5;   *h7 = rot64(*h7, 10);
  *h6 += *h8;   *h9 ^= *h6;   *h8 = rot64(*h8, 13);
  *h7 += *h9;   *h10^= *h7;   *h9 = rot64(*h9, 38);
  *h8 += *h10;  *h11^= *h8;   *h10= rot64(*h10, 53);
  *h9 += *h11;  *h0 ^= *h9;   *h11= rot64(*h11, 42);
  *h10+= *h0;   *h1 ^= *h10;  *h0 = rot64(*h0, 54);
}


static inline void
_end(uint64_t *h0, uint64_t *h1,uint64_t *h2,uint64_t *h3,
     uint64_t *h4, uint64_t *h5,uint64_t *h6,uint64_t *h7,
     uint64_t *h8, uint64_t *h9,uint64_t *h10,uint64_t *h11) {
  _end_partial(h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);
  _end_partial(h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);
  _end_partial(h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);
}


// Jenkins' spooky hash function
//
// @param[in] str     - the key
// @param[in] len     - the length of the key in bytes
//
// @return the resultant hashed value

uintptr_t
spooky_hash(const void *str, size_t len)
{
  uint64_t h[2];
  uint64_t h2, h3, h4, h5, h6, h7, h8, h9, h10, h11;
  uint64_t buf[12];
  uint64_t *endp;
  union {
    const uint8_t *p8;
    uint64_t *p64;
    uintptr_t i;
  } u;
  size_t remainder;

  h[0] = h3 = h6 = h9  = 0x9e3779b99e3779b9LL;
  h[1] = h4 = h7 = h10 = 0xdead10ccdead10ccLL;
  h2   = h5 = h8 = h11 = 0xdeadbeefdeadbeefLL;

  u.p8 = (const uint8_t *)str;
  endp = u.p64 + (len/96)*12;

  // handle all whole blocks of 96 bytes
  while (u.p64 < endp) {
    _mix(u.p64, &h[0], &h[1], &h2, &h3, &h4, &h5, &h6,
         &h7, &h8, &h9, &h10, &h11);
    u.p64 += 12;
  }

  // handle the last partial block of 96 bytes
  remainder = (len - ((const uint8_t *)endp-(const uint8_t *)str));
  memcpy(buf, endp, remainder);
  memset(((uint8_t *)buf)+remainder, 0, 96-remainder);
  ((uint8_t *)buf)[95] = remainder;
  _mix(buf, &h[0] , &h[1], &h2, &h3, &h4, &h5, &h6, &h7, &h8, &h9, &h10, &h11);

  // do some final mixing
  _end(&h[0], &h[1], &h2, &h3, &h4, &h5, &h6, &h7, &h8, &h9, &h10, &h11);
  return (uintptr_t)h[0];
}


// Hash a string.
//
uint32_t
libhpx_hash_string(const char *str, size_t len) {
  return (uint32_t)spooky_hash(str, len);
}
