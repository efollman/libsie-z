/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

typedef struct _sie_Id_Map sie_Id_Map;

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_ID_MAP_H
#define SIE_ID_MAP_H

struct _sie_Id_Map {
    sie_Context_Object parent;
    void **direct;
    size_t num_direct;
    sie_vec_declare(oflow_ids, sie_id);
    sie_vec_declare(oflow_values, void *);
    sie_id max_id;
};
SIE_CLASS_DECL(sie_Id_Map);
#define SIE_ID_MAP(p) SIE_SAFE_CAST(p, sie_Id_Map)

typedef void (sie_Id_Map_Foreach_Fn)(sie_id id, void *value, void *extra);

SIE_DECLARE(void) sie_id_map_foreach_release(sie_id id, void *object,
                                             void *extra);

SIE_DECLARE(sie_Id_Map *) sie_id_map_new(void *ctxobj, size_t initial);
SIE_DECLARE(void) sie_id_map_init(sie_Id_Map *self, void *ctxobj,
                                  size_t initial);
SIE_DECLARE(void) sie_id_map_destroy(sie_Id_Map *self);
SIE_DECLARE(void) sie_id_map_foreach(sie_Id_Map *self,
                                     sie_Id_Map_Foreach_Fn *fn, void *extra);
SIE_DECLARE(void *) sie_id_map_get(sie_Id_Map *self, sie_id id);
SIE_DECLARE(void) sie_id_map_set(sie_Id_Map *self, sie_id id, void *value);
SIE_DECLARE(sie_id) sie_id_map_get_max_id(sie_Id_Map *self);

#endif

#endif
