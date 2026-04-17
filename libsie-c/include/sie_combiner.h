/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

typedef struct _sie_Combiner sie_Combiner;

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_COMBINER_H
#define SIE_COMBINER_H

/* KLUDGE: only supports simple "1 to 1" mappings.  (i.e., one
 * sie_Output in, one sie_Output out.) */

struct _sie_Combiner {
    sie_Context_Object parent;
    size_t num_vs;
    size_t *map;
    sie_Output *output;
};
SIE_CLASS_DECL(sie_Combiner);

SIE_DECLARE(sie_Combiner *) sie_combiner_new(void *ctx_obj, size_t num_vs);
SIE_DECLARE(void) sie_combiner_init(sie_Combiner *self, void *ctx_obj, 
                                    size_t num_vs);
SIE_DECLARE(void) sie_combiner_destroy(sie_Combiner *self);

SIE_DECLARE(void) sie_combiner_add_mapping(sie_Combiner *self, 
                                           size_t in_v, size_t out_v);
SIE_DECLARE(sie_Output *) sie_combiner_combine(sie_Combiner *self, 
                                               sie_Output *input);

#endif

#endif
