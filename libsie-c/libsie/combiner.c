/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#include "sie_config.h"

#include <string.h>

#include "sie_combiner.h"

SIE_CONTEXT_OBJECT_NEW_FN(sie_combiner_new, sie_Combiner, self, ctx_obj,
                          (void *ctx_obj, size_t num_vs),
                          sie_combiner_init(self, ctx_obj, num_vs));

void sie_combiner_init(sie_Combiner *self, void *ctx_obj, size_t num_vs)
{
    size_t v;
    
    sie_context_object_init(SIE_CONTEXT_OBJECT(self), ctx_obj);
    self->num_vs = num_vs;
    self->map = sie_malloc(self, num_vs * sizeof(*self->map));
    for (v = 0; v < num_vs; v++)
        self->map[v] = -1;
    self->output = sie_output_new(self, num_vs);
}

void sie_combiner_destroy(sie_Combiner *self)
{
    sie_release(self->output);
    free(self->map);
    sie_context_object_destroy(SIE_CONTEXT_OBJECT(self));
}

SIE_CLASS(sie_Combiner, sie_Context_Object,
          SIE_MDEF(sie_destroy, sie_combiner_destroy));

void sie_combiner_add_mapping(sie_Combiner *self, size_t in_v, size_t out_v)
{
    sie_assert(out_v < self->num_vs, self);
    self->map[out_v] = in_v;
}

sie_Output *sie_combiner_combine(sie_Combiner *self, sie_Output *input)
{
    sie_Output *output;
    size_t v;
    
    output = self->output = sie_output_maybe_reuse(self->output);

    for (v = 0; v < self->num_vs; v++) {
        size_t in_v = self->map[v];
        int type;
        size_t scan;
        sie_Output_V *in_vv, *out_vv;
        sie_Output_V_Guts *in_vg, *out_vg;
        sie_assert(in_v < input->num_vs, self);
        in_vv = &input->v[in_v];
        in_vg = &input->v_guts[in_v];
        type = in_vv->type;
        sie_output_set_type(output, v, type);
        sie_output_resize(output, v, in_vg->size);
        out_vv = &output->v[v];
        out_vg = &output->v_guts[v];
        if (in_vg->size) {
            switch (type) {
            case SIE_OUTPUT_FLOAT64:
                memcpy(out_vv->float64, in_vv->float64,
                       in_vg->size * in_vg->element_size);
                break;
            case SIE_OUTPUT_RAW:
                for (scan = 0; scan < input->num_scans; scan++) {
                    out_vv->raw[scan].ptr = in_vv->raw[scan].ptr;
                    out_vv->raw[scan].size = in_vv->raw[scan].size;
                    out_vv->raw[scan].claimed = in_vv->raw[scan].claimed;
                    in_vv->raw[scan].claimed = 1;
                }
                break;
            default: sie_assert(0, self);
            }
        }
        out_vg->size = in_vg->size;
    }
    
    output->num_scans = input->num_scans;

    return output;
}
