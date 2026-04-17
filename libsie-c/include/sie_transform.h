/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

typedef struct _sie_Transform sie_Transform;

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_TRANSFORM_H
#define SIE_TRANSFORM_H

enum sie_Transform_Types {
    SIE_TRANSFORM_NONE = 0,
    SIE_TRANSFORM_LINEAR,
    SIE_TRANSFORM_MAP,
};

struct _sie_Transform {
    sie_Context *ctx;
    size_t num_vs;
    enum sie_Transform_Types *types;
    union {
        struct {
            sie_float64 scale;
            sie_float64 offset;
        } linear;
        struct {
            sie_float64 *values;
        } map;
    } *xforms;
};

SIE_DECLARE(sie_Transform *) sie_transform_new(sie_Context *ctx,
                                               size_t num_vs);
SIE_DECLARE(void) sie_transform_free(sie_Transform *xform);

SIE_DECLARE(void) sie_transform_set_none(sie_Transform *xform, size_t v);
SIE_DECLARE(void) sie_transform_set_linear(
    sie_Transform *xform, size_t v, sie_float64 scale, sie_float64 offset);
SIE_DECLARE(void) sie_transform_set_map_from_channel(
    sie_Transform *xform, size_t v, void *intake,
    sie_id chan_id, size_t chan_v);
SIE_DECLARE(void) sie_transform_apply(sie_Transform *xform,
                                      sie_Output *output);

SIE_DECLARE(void) sie_transform_set_from_xform_node(
    sie_Transform *xform, size_t v, sie_XML *node, void *intake);

#endif

#endif
