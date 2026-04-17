/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

typedef struct _sie_Spigot sie_Spigot;

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_SPIGOT_H
#define SIE_SPIGOT_H

struct _sie_Spigot {
    sie_Context_Object parent;
    int prepped;
    sie_uint64 scans;
    sie_uint64 scans_limit;
};
SIE_CLASS_DECL(sie_Spigot);
#define SIE_SPIGOT(p) SIE_SAFE_CAST(p, sie_Spigot)

SIE_DECLARE(void) sie_spigot_init(sie_Spigot *self, void *ctx_obj);
SIE_DECLARE(void) sie_spigot_destroy(sie_Spigot *self);

SIE_METHOD_DECL(sie_spigot_prep);
SIE_DECLARE(void) sie_spigot_prep(void *self);

SIE_METHOD_DECL(sie_spigot_get);
SIE_DECLARE(sie_Output *) sie_spigot_get(void *self);
SIE_METHOD_DECL(sie_spigot_get_inner);
SIE_DECLARE(sie_Output *) sie_spigot_get_inner(void *self);

SIE_METHOD_DECL(sie_spigot_clear_output);
SIE_DECLARE(void) sie_spigot_clear_output(void *self);

SIE_METHOD_DECL(sie_spigot_done);
SIE_DECLARE(int) sie_spigot_done(void *self);

SIE_DECLARE(void) sie_spigot_spigot_prep(sie_Spigot *self);
SIE_DECLARE(sie_Output *) sie_spigot_spigot_get(sie_Spigot *self);

SIE_DECLARE(void) sie_spigot_set_scan_limit(void *self, sie_uint64 limit);

#define SIE_SPIGOT_SEEK_END (~(size_t)0)
SIE_METHOD_DECL(sie_spigot_seek);
SIE_DECLARE(size_t) sie_spigot_seek(void *self, size_t target);

SIE_METHOD_DECL(sie_spigot_tell);
SIE_DECLARE(size_t) sie_spigot_tell(void *self);

SIE_METHOD_DECL(sie_binary_search);
SIE_DECLARE(int) sie_binary_search(void *self, size_t dim, sie_float64 value,
                                   size_t *block, size_t *scan);
SIE_METHOD_DECL(sie_lower_bound);
SIE_DECLARE(int) sie_lower_bound(void *self, size_t dim, sie_float64 value,
                                 size_t *block, size_t *scan);
SIE_METHOD_DECL(sie_upper_bound);
SIE_DECLARE(int) sie_upper_bound(void *self, size_t dim, sie_float64 value,
                                 size_t *block, size_t *scan);

SIE_METHOD_DECL(sie_spigot_disable_transforms);
SIE_DECLARE(void) sie_spigot_disable_transforms(void *self, int disable);
SIE_METHOD_DECL(sie_spigot_transform_output);
SIE_DECLARE(void) sie_spigot_transform_output(void *self, sie_Output *output);

#endif

#endif
