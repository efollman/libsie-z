/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

typedef struct _sie_Output sie_Output;
typedef struct _sie_Output_Struct sie_Output_Struct;
typedef struct _sie_Output_V sie_Output_V;
typedef struct _sie_Output_V_Guts sie_Output_V_Guts;
typedef struct _sie_Output_Dim sie_Output_Dim;
typedef struct _sie_Output_Raw sie_Output_Raw;

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_OUTPUT_H
#define SIE_OUTPUT_H

#include <stdio.h>

#define SIE_OUTPUT_NONE     0
#define SIE_OUTPUT_FLOAT64  1
#define SIE_OUTPUT_RAW      2

struct _sie_Output {
    sie_Context_Object parent;
    sie_Output_V_Guts *v_guts;
    /* External interface starts here */
#define SIE_OUTPUT_STRUCT_BASE(output) ((sie_Output_Struct *)&(output)->num_vs)
    size_t num_vs;
    size_t num_scans;
    size_t block;
    size_t scan_offset;
    sie_Output_V *v;
};
SIE_CLASS_DECL(sie_Output);
#define SIE_OUTPUT(p) SIE_SAFE_CAST(p, sie_Output)

/* KLUDGE much match external part of _sie_Output exactly! */
struct _sie_Output_Struct {
    size_t num_dims;
    size_t num_rows;
    size_t block;
    size_t row_offset;
    sie_Output_Dim *dim;
};

struct _sie_Output_Raw {
    void *ptr;
    size_t size;
    int claimed;
};

struct _sie_Output_V_Guts {
    size_t size;
    size_t max_size;
    size_t element_size;
};

struct _sie_Output_V {
    int type;
    sie_float64 *float64;
    sie_Output_Raw *raw;
};

/* KLUDGE must match _sie_Output_V exactly! */
struct _sie_Output_Dim {
    int type;
    sie_float64 *float64;
    sie_Output_Raw *raw;
};

SIE_DECLARE(sie_Output *) sie_output_new(void *ctx_obj, size_t num_vs);
SIE_DECLARE(void) sie_output_init(sie_Output *self,
                                  void *ctx_obj, size_t num_vs);
SIE_DECLARE(void) sie_output_destroy(sie_Output *self);
SIE_DECLARE(void) sie_output_resize(sie_Output *self, size_t v, 
                                    size_t max_size);
SIE_DECLARE(void) sie_output_grow(sie_Output *self, size_t v);
SIE_DECLARE(void) sie_output_grow_to(sie_Output *self, size_t v, size_t size);
SIE_DECLARE(void) sie_output_trim(sie_Output *self,
                                  size_t start, size_t size);
SIE_DECLARE(sie_Output *) sie_output_maybe_reuse(sie_Output *self);
SIE_DECLARE(void) sie_output_clear(sie_Output *self);
SIE_DECLARE(void) sie_output_clear_and_shrink(sie_Output *self);
SIE_DECLARE(void) sie_output_set_type(sie_Output *self, size_t v, int type);
SIE_DECLARE(void) sie_output_set_raw(sie_Output *self, size_t v, int scan,
                                     const void *data, size_t size);

SIE_DECLARE(int) sie_output_compare(sie_Output *self, sie_Output *other);
SIE_DECLARE(void) sie_output_dump(sie_Output *self, FILE *stream);

SIE_DECLARE(size_t) sie_output_get_block(sie_Output *self);
SIE_DECLARE(size_t) sie_output_get_num_dims(sie_Output *self);
SIE_DECLARE(size_t) sie_output_get_num_rows(sie_Output *self);
SIE_DECLARE(int) sie_output_get_type(sie_Output *self, size_t dim);
SIE_DECLARE(sie_Output_Struct *) sie_output_get_struct(sie_Output *self);
SIE_DECLARE(sie_float64 *) sie_output_get_float64(sie_Output *self,
                                                  size_t dim);
SIE_DECLARE(sie_Output_Raw *) sie_output_get_raw(sie_Output *self,
                                                 size_t dim);
#endif

#endif
