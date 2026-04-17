/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#define SIE_VEC_CONTEXT_OBJECT ctx_obj

#include "sie_config.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "sie_config.h"
#include "sie_vec.h"

static void *vec_calloc(size_t size)
{
    void *ptr = calloc(1, size + sizeof(sie_vec_Metadata));
    return ptr ? sie_vec_alloc_to_vec(ptr) : NULL;
}

static void *vec_realloc(void *v, size_t size)
{
    if (v && sie_vec_metadata(v)->class_ &&
        sie_vec_metadata(v)->class_->v_realloc) {
        return sie_vec_metadata(v)->class_->v_realloc(v, size);
    } else {
        void *src = v ? sie_vec_vec_to_alloc(v) : NULL;
        void *ptr = realloc(src, size + sizeof(sie_vec_Metadata));
        return ptr ? sie_vec_alloc_to_vec(ptr) : NULL;
    }
}

void *_sie_vec_resize(void *ctx_obj, void *v, size_t capacity, size_t el_size)
{
    if (v) {
        size_t old_capacity = sie_vec_capacity(v);
        v = vec_realloc(v, capacity * el_size);
        if (!v) {
            if (ctx_obj)
                sie_throw_oom(ctx_obj);
            else
                abort();
        }
        if (capacity > old_capacity)
            memset((char *)v + old_capacity * el_size, 0,
                   (capacity - old_capacity) * el_size);
    } else {
        v = vec_calloc(capacity * el_size);
        if (!v) {
            if (ctx_obj)
                sie_throw_oom(ctx_obj);
            else
                abort();
        }
    }

    sie_vec_raw_capacity(v) = capacity;
    if (sie_vec_raw_size(v) > capacity)
        sie_vec_raw_size(v) = capacity;

    return v;
}

void *_sie_vec_grow(void *ctx_obj, void *v, size_t target, size_t el_size)
{
    size_t capacity = sie_vec_capacity(v);

    if (capacity == 0) capacity = 64 / el_size;
    if (capacity == 0) capacity = 1;
    while (capacity < target)
        capacity += (1 + capacity) >> 1;

    v = _sie_vec_resize(ctx_obj, v, capacity, el_size);

    return v;
}

void sie_vec_vstrcatf(void *ctx_obj, char **v, const char *fmt, va_list args)
{
    char buf[1024];
    int len;
    char *end;
    va_list args2;

    va_copy(args2, args);
    len = apr_vsnprintf(buf, sizeof(buf), fmt, args2);
    va_end(args2);

    sie_vec_reserve(*v, sie_vec_size(*v) + len + 1);

    end = *v + sie_vec_size(*v);
    if (len >= sizeof(buf))
        apr_vsnprintf(end, len + 1, fmt, args);
    else
        memcpy(end, buf, len + 1);
    sie_vec_raw_size(*v) += len;
}

void sie_vec_strcatf(void *ctx_obj, char **v, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    sie_vec_vstrcatf(ctx_obj, v, fmt, args);
    va_end(args);
}

void sie_vec_vprintf(void *ctx_obj, char **v, const char *fmt, va_list args)
{
    if (*v)
        sie_vec_raw_size(*v) = 0;
    sie_vec_vstrcatf(ctx_obj, v, fmt, args);
}

void sie_vec_printf(void *ctx_obj, char **v, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    sie_vec_vprintf(ctx_obj, v, fmt, args);
    va_end(args);
}

void sie_vec_memcat(void *ctx_obj, char **v, const void *data, size_t len)
{
    size_t sz = sie_vec_size(*v);
    sie_vec_reserve(*v, sz + len + 1);
    memcpy(*v + sz, data, len);
    (*v)[sz + len] = 0;
    sie_vec_raw_size(*v) += len;
}

void sie_vec_free(void *v)
{
    if (v) {
        if ((sie_vec_metadata(v)->class_ &&
             sie_vec_metadata(v)->class_->v_free)) {
            sie_vec_metadata(v)->class_->v_free(v);
        } else {
            free(sie_vec_vec_to_alloc(v));
        }
    }
}

void _sie_vec_print(void *v)
{
    if (!v) {
        printf("%p is the null vector.\n", v);
    } else {
        printf("%p is a %s vector:\n",
               v, sie_vec_metadata(v)->class_ ? "custom" : "standard");
        printf("    size: %"APR_SIZE_T_FMT"\n", sie_vec_size(v)); 
        printf("capacity: %"APR_SIZE_T_FMT"\n", sie_vec_capacity(v));
        if (sie_vec_metadata(v)->class_) {
            printf("   class: %p\n", sie_vec_metadata(v)->class_); 
            printf("userdata: %p\n", sie_vec_metadata(v)->userdata);
            if (sie_vec_metadata(v)->class_->v_print)
                sie_vec_metadata(v)->class_->v_print(v);
        }
    }
}
