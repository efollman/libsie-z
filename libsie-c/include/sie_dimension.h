/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

typedef struct _sie_Dimension sie_Dimension;

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_DIMENSION_H
#define SIE_DIMENSION_H

struct _sie_Dimension {
    sie_Ref parent;
    sie_Intake *intake;
    sie_XML *xml;
    sie_id index;
    sie_id group;
    sie_id decoder_id;
    size_t decoder_v;
    sie_XML *xform_node;
};
SIE_CLASS_DECL(sie_Dimension);
#define SIE_DIMENSION(p) SIE_SAFE_CAST(p, sie_Dimension)

SIE_DECLARE(sie_Dimension *) sie_dimension_new(void *intake, sie_XML *node,
                                               sie_id toplevel_group);
SIE_DECLARE(void) sie_dimension_init(sie_Dimension *self, void *intake,
                                     sie_XML *node, sie_id toplevel_group);
SIE_DECLARE(void) sie_dimension_destroy(sie_Dimension *self);

SIE_DECLARE(void) sie_dimension_dump(sie_Dimension *self, FILE *stream);
SIE_DECLARE(sie_Iterator *) sie_dimension_get_tags(sie_Dimension *self);
SIE_DECLARE(sie_id) sie_dimension_get_index(sie_Dimension *self);

#endif

#endif
