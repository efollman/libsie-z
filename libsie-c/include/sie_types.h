/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_TYPES_H
#define SIE_TYPES_H

typedef sie_uint32    sie_id;
#define SIE_NULL_ID   (~(sie_id)0)

#define sie_get_int8(p)    (*(sie_int8 *)(p))
#define sie_get_int16(p)   (*(sie_int16 *)(p))
#define sie_get_int32(p)   (*(sie_int32 *)(p))
#define sie_get_int64(p)   (*(sie_int64 *)(p))
#define sie_get_uint8(p)   (*(sie_uint8 *)(p))
#define sie_get_uint16(p)  (*(sie_uint16 *)(p))
#define sie_get_uint32(p)  (*(sie_uint32 *)(p))
#define sie_get_uint64(p)  (*(sie_uint64 *)(p))
#define sie_get_float32(p) (*(sie_float32 *)(p))
#define sie_get_float64(p) (*(sie_float64 *)(p))

#define sie_set_int8(p, v)    (*(sie_int8 *)(p) = (v))
#define sie_set_int16(p, v)   (*(sie_int16 *)(p) = (v))
#define sie_set_int32(p, v)   (*(sie_int32 *)(p) = (v))
#define sie_set_int64(p, v)   (*(sie_int64 *)(p) = (v))
#define sie_set_uint8(p, v)   (*(sie_uint8 *)(p) = (v))
#define sie_set_uint16(p, v)  (*(sie_uint16 *)(p) = (v))
#define sie_set_uint32(p, v)  (*(sie_uint32 *)(p) = (v))
#define sie_set_uint64(p, v)  (*(sie_uint64 *)(p) = (v))
#define sie_set_float32(p, v) (*(sie_float32 *)(p) = (v))
#define sie_set_float64(p, v) (*(sie_float64 *)(p) = (v))

#endif

#endif
