/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

#elif defined(SIE_INCLUDE_BODIES)

#ifndef _SIE_VEC_H
#define _SIE_VEC_H

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

typedef struct sie_vec_Class {
    void *(*v_realloc)(void *vec, size_t new_size);
    void (*v_free)(void *vec);
    void (*v_print)(void *vec);
} sie_vec_Class;

typedef struct sie_vec_Metadata {
    size_t size;
    size_t capacity;
    sie_vec_Class *class_;
    void *userdata;
} sie_vec_Metadata;

/* HORRIBLE KLUDGE.  To be able to safely throw an error when an OOM
 * occurs.  I chose to do this kludge rather than change hundreds of
 * lines of code. */
#ifndef SIE_VEC_CONTEXT_OBJECT
#define SIE_VEC_CONTEXT_OBJECT self
#endif

#define sie_vec_metadata(v) (((sie_vec_Metadata *)(v)) - 1)
#define sie_vec_vec_to_alloc(v) (((sie_vec_Metadata *)(v)) - 1)
#define sie_vec_alloc_to_vec(a) ((void *)(((sie_vec_Metadata *)(a)) + 1))
#define sie_vec_raw_size(v) (sie_vec_metadata(v)->size)
#define sie_vec_raw_capacity(v) (sie_vec_metadata(v)->capacity)
#define sie_vec_size(v) ((v) ? sie_vec_raw_size(v) : 0)
#define sie_vec_capacity(v) ((v) ? sie_vec_raw_capacity(v) : 0)
#define sie_vec_el_size(v) (sizeof(*(v)))

#define sie_vec_octets(v) (sie_vec_size(v) * sie_vec_el_size(v))

#define sie_vec(v, type, max) \
    type *v = (void *)0

#define sie_vec_declare(v, type) \
    type *v

#define _sie_vec_init(v, max) \
    v = (void *)0

#define sie_vec_typeof(v) \
    __typeof__((v)[0])

#define sie_vec_iterator(v, i) \
    sie_vec_typeof(v) *i

#define sie_vec_current(v) \
    ((v) + sie_vec_size(v))

#define sie_vec_clear(v) \
    do { if (v) { sie_vec_raw_size(v) = 0; } } while (0)

#define sie_vec_empty(v) \
    (sie_vec_size(v) == 0)

#define sie_vec_zero(v) \
    (memset((v), 0, sie_vec_capacity(v) * sie_vec_el_size(v)))

SIE_DECLARE(void *) _sie_vec_resize(void *ctx_obj, void *v,
                                    size_t capacity, size_t el_size);
SIE_DECLARE(void *) _sie_vec_grow(void *ctx_obj, void *v,
                                  size_t amount, size_t el_size);

#define sie_vec_resize(v, count)                        \
    ((v) = _sie_vec_resize(SIE_VEC_CONTEXT_OBJECT, v, count, sizeof(*(v))))

#define sie_vec_reserve(v, count)                                         \
    ((v) = (((count) > sie_vec_capacity(v))                              \
            ? _sie_vec_resize(SIE_VEC_CONTEXT_OBJECT, v, count, sizeof(*(v)))                     \
            : (v)))

#define sie_vec_grow(v, target) \
    ((v) = (((target) > sie_vec_capacity(v)) \
            ? _sie_vec_grow(SIE_VEC_CONTEXT_OBJECT, v, target, sizeof(*(v))) \
            : (v)))

#define sie_vec_set_size(v, count) ( \
     sie_vec_reserve(v, (count)), \
     sie_vec_raw_size(v) = (count) \
)

#define sie_vec_append(v, items, count)                                 \
    (sie_vec_grow(v, sie_vec_size(v) + (count)),                        \
     memmove(sie_vec_current(v), (items), (count) * sie_vec_el_size(v)), \
     sie_vec_raw_size(v) = sie_vec_raw_size(v) + (count))

#define sie_vec_next(v)                         \
    (sie_vec_grow(v, sie_vec_size(v) + 1),       \
     (v) + sie_vec_raw_size(v)++)

#define sie_vec_front(v) \
    (sie_vec_size(v) ? (v) : 0)

#define sie_vec_back(v) \
    (sie_vec_size(v) ? (v) + sie_vec_size(v) - 1 : 0)

#define sie_vec_push_back(v, value) \
    (*sie_vec_next(v) = (value))

#define sie_vec_pop_back(v) \
    (sie_vec_size(v) ? --sie_vec_raw_size(v) : 0)

#define sie_vec_forall(v, i) \
    for ((i) = (v); (i) < ((v) + sie_vec_size(v)); ++(i))

#define sie_vec_value(v, i) \
    (v)[i]

#define sie_vec_index(v, p) \
    ((p) - (v))

SIE_DECLARE(void) sie_vec_free(void *v);

SIE_DECLARE(void) sie_vec_vstrcatf(void *ctx_obj, char **v, const char *fmt, va_list args);
SIE_DECLARE_NONSTD(void) sie_vec_strcatf(void *ctx_obj, char **v, const char *fmt, ...)
    __gcc_attribute__ ((format(printf, 3, 4)));
SIE_DECLARE(void) sie_vec_vprintf(void *ctx_obj, char **v, const char *fmt, va_list args);
SIE_DECLARE_NONSTD(void) sie_vec_printf(void *ctx_obj, char **v, const char *fmt, ...)
    __gcc_attribute__ ((format(printf, 3, 4)));
SIE_DECLARE(void) sie_vec_memcat(void *ctx_obj, char **v, const void *data, size_t len);

/* KLUDGE not done */
#define sie_vec_splice(v, i, len, rep, replen) ( \
    sie_vec_reserve(v, (replen) - (len)), \
)

#define sie_vec_overwrite(dest, src) \
    do { \
        (dest) = (src); \
    } while (0)

SIE_DECLARE(void) _sie_vec_print(void *v);

#endif /* _SIE_VEC_H */

#endif
