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

#ifndef HPX_TYPES_H
#define HPX_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ffi.h>
#include <hpx/builtins.h>
#include <hpx/attributes.h>

/// @file include/hpx/types.h

/// Extern HPX macros
/// @{
typedef short hpx_status_t;

#define  HPX_ERROR           ((hpx_status_t)-1)
#define  HPX_SUCCESS         ((hpx_status_t)0)
#define  HPX_RESEND          ((hpx_status_t)1)
#define  HPX_LCO_ERROR       ((hpx_status_t)2)
#define  HPX_LCO_CHAN_EMPTY  ((hpx_status_t)3)
#define  HPX_LCO_TIMEOUT     ((hpx_status_t)4)
#define  HPX_LCO_RESET       ((hpx_status_t)5)
#define  HPX_ENOMEM          ((hpx_status_t)6)
#define  HPX_USER            ((hpx_status_t)127)

const char *hpx_strerror(hpx_status_t) HPX_PUBLIC;

/// @}

/// An HPX datatype
typedef ffi_type* hpx_type_t;

/// HPX basic datatypes
/// @{
#define HPX_CHAR               &ffi_type_schar
#define HPX_UCHAR              &ffi_type_uchar
#define HPX_SCHAR              &ffi_type_schar
#define HPX_SHORT              &ffi_type_sshort
#define HPX_USHORT             &ffi_type_ushort
#define HPX_SSHORT             &ffi_type_sshort
#define HPX_INT                &ffi_type_sint
#define HPX_UINT               &ffi_type_uint
#define HPX_SINT               &ffi_type_sint
#define HPX_LONG               &ffi_type_slong
#define HPX_ULONG              &ffi_type_ulong
#define HPX_SLONG              &ffi_type_slong
#define HPX_VOID               &ffi_type_void
#define HPX_UINT8              &ffi_type_uint8
#define HPX_SINT8              &ffi_type_sint8
#define HPX_UINT16             &ffi_type_uint16
#define HPX_SINT16             &ffi_type_sint16
#define HPX_UINT32             &ffi_type_uint32
#define HPX_SINT32             &ffi_type_sint32
#define HPX_UINT64             &ffi_type_uint64
#define HPX_SINT64             &ffi_type_sint64
#define HPX_FLOAT              &ffi_type_float
#define HPX_DOUBLE             &ffi_type_double
#define HPX_POINTER            &ffi_type_pointer
#define HPX_LONGDOUBLE         &ffi_type_longdouble
#define HPX_COMPLEX_FLOAT      &ffi_type_complex_float
#define HPX_COMPLEX_DOUBLE     &ffi_type_complex_double
#define HPX_COMPLEX_LONGDOUBLE &ffi_type_complex_longdouble

#ifdef __LP64__
#define HPX_SIZE_T             HPX_UINT64
#else
#define HPX_SIZE_T             HPX_UINT32
#endif

#define HPX_ADDR               HPX_UINT64
#define HPX_ACTION_T           HPX_UINT16
/// @}

#ifdef __cplusplus
}
#endif

#endif
