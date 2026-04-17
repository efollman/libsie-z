/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#include "sie_config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "sie_output.h"

SIE_CONTEXT_OBJECT_NEW_FN(sie_output_new, sie_Output, self, ctx_obj,
                          (void *ctx_obj, size_t num_vs),
                          sie_output_init(self, ctx_obj, num_vs));

void sie_output_init(sie_Output *self, void *ctx_obj, size_t num_vs)
{
    sie_context_object_init(SIE_CONTEXT_OBJECT(self), ctx_obj);
    self->num_vs = num_vs;
    self->v = sie_calloc(self, num_vs * sizeof(*self->v));
    self->v_guts = sie_calloc(self, num_vs * sizeof(*self->v_guts));
}

void sie_output_destroy(sie_Output *self)
{
    size_t v;
    sie_output_clear(self);
    for (v = 0; v < self->num_vs; v++) {
        free(self->v[v].float64);
        free(self->v[v].raw);
    }
    free(self->v_guts);
    free(self->v);
    sie_context_object_destroy(SIE_CONTEXT_OBJECT(self));
}

void *sie_output_copy(sie_Output *self)
{
    sie_Output *copy = sie_context_object_copy(SIE_CONTEXT_OBJECT(self));
    size_t row, v;
    copy->v = sie_calloc(self, self->num_vs * sizeof(*copy->v));
    copy->v_guts = sie_calloc(self, self->num_vs * sizeof(*copy->v_guts));
    for (v = 0; v < self->num_vs; v++) {
        sie_Output_V *sv = &self->v[v];
        sie_Output_V *cv = &copy->v[v];
        sie_Output_V_Guts *svg = &self->v_guts[v];
        sie_Output_V_Guts *cvg = &copy->v_guts[v];
        cv->type = sv->type;
        cvg->size = svg->size;
        cvg->max_size = svg->size;
        cvg->element_size = svg->element_size;
        switch (cv->type) {
        case SIE_OUTPUT_RAW:
            cv->raw = sie_malloc(self, cvg->element_size * cvg->max_size);
            for (row = 0; row < cvg->size; row++) {
                cv->raw[row].claimed = 0;
                cv->raw[row].size = sv->raw[row].size;
                cv->raw[row].ptr = sie_malloc(self, cv->raw[row].size);
                memcpy(cv->raw[row].ptr, sv->raw[row].ptr, cv->raw[row].size);
            }
            break;
        case SIE_OUTPUT_FLOAT64:
            cv->float64 = sie_malloc(self, cvg->element_size * cvg->max_size);
            memcpy(cv->float64, sv->float64, cvg->element_size * cvg->size);
            break;
        default: sie_errorf((self, "unknown type to copy"));
        }
    }
    return copy;
}

SIE_CLASS(sie_Output, sie_Context_Object,
          SIE_MDEF(sie_destroy, sie_output_destroy)
          SIE_MDEF(sie_copy, sie_output_copy));


void sie_output_resize(sie_Output *self, size_t v, size_t max_size)
{
    sie_Output_V *vv = &self->v[v];
    sie_Output_V_Guts *vg = &self->v_guts[v];
    void **ptr = NULL;

    if (vg->size == 0 && max_size == 0)
        return;

    sie_assert(vg->size < max_size, self);
    vg->max_size = max_size;

    switch (vv->type) {
    case SIE_OUTPUT_RAW:     ptr = (void *)&vv->raw;     break;
    case SIE_OUTPUT_FLOAT64: ptr = (void *)&vv->float64; break;
    default: sie_errorf((self, "unknown type to grow"));
    }
    
    *ptr = sie_realloc(self, *ptr, vg->element_size * vg->max_size);
}

void sie_output_grow_to(sie_Output *self, size_t v, size_t size)
{
    sie_Output_V_Guts *vg = &self->v_guts[v];

    if (vg->max_size < size)
        sie_output_resize(self, v, size);
}

void sie_output_grow(sie_Output *self, size_t v)
{
    sie_Output_V_Guts *vg = &self->v_guts[v];

    if (vg->max_size == 0)
        sie_output_resize(self, v, 16);
    else
        sie_output_resize(self, v, vg->max_size * 2);
}

static void clear_raw(sie_Output_V *vv, size_t index)
{
    if (!vv->raw[index].claimed)
        free(vv->raw[index].ptr);
    vv->raw[index].ptr = NULL;
    vv->raw[index].size = 0;
    vv->raw[index].claimed = 0;
}

void sie_output_trim(sie_Output *self, size_t start, size_t size)
{
    size_t i;
    size_t old_size = self->num_scans;
    sie_assert(start + size <= old_size, self); 
    for (i = 0; i < self->num_vs; i++) {
        sie_Output_V *vv = &self->v[i];
        sie_Output_V_Guts *vg = &self->v_guts[i];
        size_t j;
        vg->size = size;
        switch (vv->type) {
        case SIE_OUTPUT_RAW:
            for (j = 0; j < start; j++)
                clear_raw(vv, j);
            for (j = start + size; j < old_size; j++)
                clear_raw(vv, j);
            if (start != 0)
                memmove(vv->raw, vv->raw + start,
                        sizeof(*vv->raw) * size);
            memset(vv->raw + size, 0, sizeof(*vv->raw) * (old_size - size));
            break;
        case SIE_OUTPUT_FLOAT64:
            if (start != 0)
                memmove(vv->float64, vv->float64 + start,
                        sizeof(*vv->float64) * size);
            break;
        }
    }
    self->num_scans = size;
}

sie_Output *sie_output_maybe_reuse(sie_Output *self)
{
    if (sie_refcount(self) == 1) {
        sie_output_clear(self);
        return self;
    } else {
        sie_Output *new_output = sie_output_new(self, self->num_vs);
        sie_release(self);
        return new_output;
    }
}

void sie_output_clear(sie_Output *self)
{
    size_t v;

    self->num_scans = 0;
    
    for (v = 0; v < self->num_vs; v++) {
        sie_Output_V *vv = &self->v[v];
        sie_Output_V_Guts *vg = &self->v_guts[v];
        if (vv->type == SIE_OUTPUT_RAW) {
            size_t i;
            for (i = 0; i < vg->size; i++) 
                clear_raw(vv, i);
        }
        vg->size = 0;
    }
}

void sie_output_clear_and_shrink(sie_Output *self)
{
    size_t v;

    sie_output_clear(self);

    for (v = 0; v < self->num_vs; v++) {
        sie_Output_V *vv = &self->v[v];
        sie_Output_V_Guts *vg = &self->v_guts[v];
        free(vv->float64);
        vv->float64 = NULL;
        free(vv->raw);
        vv->raw = NULL;
        vg->max_size = 0;
        vg->size = 0;
    }
}

void sie_output_set_type(sie_Output *self, size_t v, int type)
{
    self->v[v].type = type;
    switch (type) {
    case SIE_OUTPUT_FLOAT64:
        self->v_guts[v].element_size = sizeof(*self->v[v].float64);
        break;
    case SIE_OUTPUT_RAW:
        self->v_guts[v].element_size = sizeof(*self->v[v].raw);
        break;
    }
}

void sie_output_set_raw(sie_Output *self, size_t v, int scan,
                        const void *data, size_t size)
{
    sie_Output_V *vv = &self->v[v];
    vv->raw[scan].ptr = sie_malloc(self, size);
    vv->raw[scan].size = size;
    vv->raw[scan].claimed = 0;
    memcpy(vv->raw[scan].ptr, data, size);
}

int sie_output_compare(sie_Output *self, sie_Output *other)
{
    size_t v, scan;

    if (self->num_vs != other->num_vs)
        return 0;
    if (self->num_scans != other->num_scans)
        return 0;
    for (v = 0; v < self->num_vs; v++) {
        if (self->v[v].type != other->v[v].type)
            return 0;
        switch (self->v[v].type) {
        case SIE_OUTPUT_FLOAT64:
            for (scan = 0; scan < self->num_scans; scan++) {
                if (self->v[v].float64[scan] != other->v[v].float64[scan])
                    return 0;
            }
            break;
        case SIE_OUTPUT_RAW: {
            for (scan = 0; scan < self->num_scans; scan++) {
                if (self->v[v].raw[scan].size != other->v[v].raw[scan].size)
                    return 0;
                if (memcmp(self->v[v].raw[scan].ptr,
                           other->v[v].raw[scan].ptr,
                           self->v[v].raw[scan].size))
                    return 0;
            }
            break;
        }
        }
    }
    return 1;
}

void sie_output_dump(sie_Output *self, FILE *stream)
{
    size_t v, scan, i;
    char float_buf[128];

    for (scan = 0; scan < self->num_scans; scan++) {
        fprintf(stream, "scan[%"APR_SIZE_T_FMT"]: ", scan);
        for (v = 0; v < self->num_vs; v++) {
            switch (self->v[v].type) {
            case SIE_OUTPUT_FLOAT64:
                apr_snprintf(float_buf, sizeof(float_buf), "%g",
                             self->v[v].float64[scan]);
                fprintf(stream, "v[%"APR_SIZE_T_FMT"] = %s; ",
                        v, float_buf);
                break;
            case SIE_OUTPUT_RAW: {
                unsigned char *cptr = self->v[v].raw[scan].ptr;
                fprintf(stream, "v[%"APR_SIZE_T_FMT"] =", v);
                for (i = 0; i < self->v[v].raw[scan].size; i++)
                    fprintf(stream, " %02x", cptr[i]);
                fprintf(stream, "; ");
                break;
            }
            }
        }
        fprintf(stream, "\n");
    }
}

size_t sie_output_get_block(sie_Output *self)
{
    if (self)
        return self->block;
    else
        return 0;
}

size_t sie_output_get_num_dims(sie_Output *self)
{
    if (self)
        return self->num_vs;
    else
        return 0;
}

size_t sie_output_get_num_rows(sie_Output *self)
{
    if (self)
        return self->num_scans;
    else
        return 0;
}

int sie_output_get_type(sie_Output *self, size_t dim)
{
    if (self && dim < self->num_vs)
        return self->v[dim].type;
    else
        return SIE_OUTPUT_NONE;
}

sie_Output_Struct *sie_output_get_struct(sie_Output *self)
{
    if (self)
        return SIE_OUTPUT_STRUCT_BASE(self);
    else
        return NULL;
}

sie_float64 *sie_output_get_float64(sie_Output *self, size_t dim)
{
    if (self && dim < self->num_vs)
        return self->v[dim].float64;
    else
        return NULL;
}

sie_Output_Raw *sie_output_get_raw(sie_Output *self, size_t dim)
{
    if (self && dim < self->num_vs)
        return self->v[dim].raw;
    else
        return NULL;
}
