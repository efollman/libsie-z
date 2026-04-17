/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#include "sie_config.h"

#include <stdlib.h>

#include "sie_context.h"
#include "sie_vec.h"

#if 0
static void *sie_vec_realloc(void *v, size_t new_size)
{
    sie_Context *ctx = sie_vec_metadata(v)->userdata;
    void *ptr = sie_realloc(ctx, sie_vec_vec_to_alloc(v),
                            new_size + sizeof(sie_vec_Metadata));
    return ptr ? sie_vec_alloc_to_vec(ptr) : NULL;
}

static void sie_vec_free(void *v)
{
    sie_Context *ctx = sie_vec_metadata(v)->userdata;
    sie_ctx_free(ctx, sie_vec_vec_to_alloc(v));
}

static sie_vec_Class sie_vec = {
    NULL, /* v_realloc */
    NULL, /* v_free */
    NULL, /* v_print */
};
#endif

void *sie_vec_init(void *ctx_obj)
{
#if 0
    void *ptr = calloc(ctx, sizeof(sie_vec_Metadata) + 4);
    void *v = sie_vec_alloc_to_vec(ptr);
    if (!ptr) return NULL;
    sie_vec_metadata(v)->class_ = &sie_vec;
    sie_vec_metadata(v)->userdata = sie_context(ctx_obj);
    return v;
#endif
    return NULL;
}

static void free_deref(void *ptr_v)
{
    void **ptr = ptr_v;
    sie_vec_free(*ptr);
}

void sie_vec_autofree(void *ctx_obj, void *vec_p)
{
    sie_cleanup_push(ctx_obj, free_deref, vec_p);
}
