/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#define SIE_VEC_CONTEXT_OBJECT xform

#include "sie_config.h"

#include "sie_transform.h"
#include "sie_vec.h"
#include "sie_sie_vec.h"
#include "sie_strtod.h"

sie_Transform *sie_transform_new(sie_Context *ctx, size_t num_vs)
{
    sie_Transform *xform = sie_calloc(ctx, sizeof(*xform));
    
    xform->ctx = ctx;
    xform->num_vs = num_vs;
    xform->types = sie_calloc(ctx, num_vs * sizeof(*xform->types));
    xform->xforms = sie_calloc(ctx, num_vs * sizeof(*xform->xforms));
    
    return xform;
}

void sie_transform_free(sie_Transform *xform)
{
    size_t v;
    for (v = 0; v < xform->num_vs; v++) {
        if (xform->types[v] == SIE_TRANSFORM_MAP)
            sie_vec_free(xform->xforms[v].map.values);
    }
    free(xform->types);
    free(xform->xforms);
    free(xform);
}

void sie_transform_set_none(sie_Transform *xform, size_t v)
{
    sie_assert(v < xform->num_vs, xform->ctx);
    xform->types[v] = SIE_TRANSFORM_NONE;
}

void sie_transform_set_linear(sie_Transform *xform, size_t v,
                              sie_float64 scale, sie_float64 offset)
{
    sie_assert(v < xform->num_vs, xform->ctx);
    xform->types[v] = SIE_TRANSFORM_LINEAR;
    xform->xforms[v].linear.scale = scale;
    xform->xforms[v].linear.offset = offset;
}

void sie_transform_set_map_from_channel(sie_Transform *xform, size_t v,
                                        void *intake,
                                        sie_id chan_id, size_t chan_v)
{
    size_t mark = sie_cleanup_mark(xform->ctx);
    sie_Channel *channel =
        sie_autorelease(sie_get_channel(intake, chan_id));
    sie_Spigot *spigot = sie_autorelease(sie_attach_spigot(channel));
    sie_Output *output;

    sie_assert(spigot, xform->ctx);
    sie_assert(v < xform->num_vs, xform->ctx);
    xform->types[v] = SIE_TRANSFORM_MAP;
    xform->xforms[v].map.values = sie_vec_init(xform->ctx);

    while ((output = sie_spigot_get(spigot))) {
        size_t scan;
        sie_assert(chan_v < output->num_vs, xform->ctx);
        sie_assert(output->v[chan_v].type == SIE_OUTPUT_FLOAT64, xform->ctx);
        for (scan = 0; scan < output->num_scans; scan++)
            sie_vec_push_back(xform->xforms[v].map.values,
                              output->v[chan_v].float64[scan]);
    }

    sie_cleanup_pop_mark(xform->ctx, mark);
}

void sie_transform_set_from_xform_node(sie_Transform *xform,
                                       size_t v, sie_XML *node,
                                       void *intake)
{
    sie_assert(node, xform->ctx);
    sie_assert(node->type == SIE_XML_ELEMENT, xform->ctx);
    sie_assert(node->value.element.name == sie_literal(node, xform),
               xform->ctx);

    {
        sie_String *scale_s = sie_xml_get_attribute_literal(node, scale);
        sie_String *offset_s = sie_xml_get_attribute_literal(node, offset);
        sie_String *map_s = sie_xml_get_attribute_literal(node, map);
        sie_String *channel_s = sie_xml_get_attribute_literal(node, channel);
        sie_String *dimension_s =
            sie_xml_get_attribute_literal(node, dimension);
        sie_String *index_ch_s =
            sie_xml_get_attribute_literal(node, index_ch);
        sie_String *index_dim_s =
            sie_xml_get_attribute_literal(node, index_dim);
    
        if (scale_s && offset_s) {
            sie_assert(!map_s && !channel_s && !dimension_s, xform->ctx);
            sie_transform_set_linear(xform, v,
                                     sie_strtod(sie_sv(scale_s), NULL),
                                     sie_strtod(sie_sv(offset_s), NULL));
        } else if (index_ch_s && index_dim_s) {
            sie_transform_set_map_from_channel(
                xform, v, intake,
                sie_strtoid(node, sie_sv(index_ch_s)),
                sie_strtosizet(node, sie_sv(index_dim_s)));
        } else if (map_s) {
            if (map_s == sie_literal(node, index)) {
                sie_assert(channel_s && dimension_s, node);
                sie_transform_set_map_from_channel(
                    xform, v, intake,
                    sie_strtoid(node, sie_sv(channel_s)),
                    sie_strtosizet(node, sie_sv(dimension_s)));
            } else {
                sie_errorf((xform->ctx, "unknown map type '%s'",
                            sie_sv(map_s)));
            }
        } else {
            sie_errorf((xform->ctx, "bad args to transform"));
        }
    }
}

void sie_transform_apply(sie_Transform *xform, sie_Output *output)
{
    size_t v, scan;
    sie_assert(output->num_vs == xform->num_vs, xform->ctx);
    if (!output->num_scans)
        return;
    for (v = 0; v < xform->num_vs; v++) {
        switch (xform->types[v]) {
        case SIE_TRANSFORM_NONE:
            break;
        case SIE_TRANSFORM_LINEAR:
            sie_assert(output->v[v].type == SIE_OUTPUT_FLOAT64, xform->ctx);
            for (scan = 0; scan < output->num_scans; scan++) {
                output->v[v].float64[scan] =
                    output->v[v].float64[scan] *
                    xform->xforms[v].linear.scale +
                    xform->xforms[v].linear.offset;
            }
            break;
        case SIE_TRANSFORM_MAP:
            sie_assert(output->v[v].type == SIE_OUTPUT_FLOAT64, xform->ctx);
            for (scan = 0; scan < output->num_scans; scan++) {
                size_t index = (size_t)output->v[v].float64[scan];
                sie_assert(index < sie_vec_size(xform->xforms[v].map.values),
                           xform->ctx);
                output->v[v].float64[scan] =
                    xform->xforms[v].map.values[index];
            }
            break;
        }
    }
}
