/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

typedef struct _sie_Iterator sie_Iterator;
typedef struct _sie_Vec_Iterator sie_Vec_Iterator;
typedef struct _sie_Id_Map_Iterator sie_Id_Map_Iterator;
typedef struct _sie_XML_Iterator sie_XML_Iterator;

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_ITERATOR_H
#define SIE_ITERATOR_H

struct _sie_Iterator {
    sie_Context_Object parent;
};
SIE_CLASS_DECL(sie_Iterator);
#define SIE_ITERATOR(p) SIE_SAFE_CAST(p, sie_Iterator)

SIE_METHOD_DECL(sie_iterator_next);
SIE_DECLARE(void *) sie_iterator_next(void *self);
SIE_DECLARE(void) sie_iterator_init(sie_Iterator *self, void *ctx_obj);
SIE_DECLARE(void) sie_iterator_destroy(sie_Iterator *self);

/*
void sie_iterator_load_current(void *self);
void sie_iterator_release_current(void *self);
//void sie_iterator_iterator_load_current(sie_Iterator *self);
//void sie_iterator_iterator_release_current(sie_Iterator *self);
void *sie_iterator_iterator_next(sie_Iterator *self);
*/

struct _sie_Vec_Iterator {
    sie_Iterator parent;
    void *grip;
    void **vec;
    void *cur;
    ssize_t index;
};
SIE_CLASS_DECL(sie_Vec_Iterator);
#define SIE_VEC_ITERATOR(p) SIE_SAFE_CAST(p, sie_Vec_Iterator)

SIE_DECLARE(sie_Vec_Iterator *) sie_vec_iterator_new(void *ctx_obj, void *grip,
                                                     void *vec);
SIE_DECLARE(void) sie_vec_iterator_init(sie_Vec_Iterator *self, void *ctx_obj,
                                        void *grip, void *vec);
SIE_DECLARE(void) sie_vec_iterator_destroy(sie_Vec_Iterator *self);
SIE_DECLARE(void *) sie_vec_iterator_iterator_next(sie_Vec_Iterator *self);

typedef void *(sie_Id_Map_Realize_Fn)(void *ctx_obj, void *thing, void *data);

struct _sie_Id_Map_Iterator {
    sie_Iterator parent;
    sie_Id_Map *id_map;
    sie_Id_Map_Realize_Fn *realize;
    void *data;
    int overflow;
    void *cur;
    ssize_t index;
};
SIE_CLASS_DECL(sie_Id_Map_Iterator);
#define SIE_ID_MAP_ITERATOR(p) SIE_SAFE_CAST(p, sie_Id_Map_Iterator)

SIE_DECLARE(sie_Id_Map_Iterator *) sie_id_map_iterator_new(
    sie_Id_Map *id_map, sie_Id_Map_Realize_Fn *realize, void *data);
SIE_DECLARE(void) sie_id_map_iterator_init(
    sie_Id_Map_Iterator *self, sie_Id_Map *id_map,
    sie_Id_Map_Realize_Fn *realize, void *data);
SIE_DECLARE(void) sie_id_map_iterator_destroy(sie_Id_Map_Iterator *self);

typedef void *(sie_XML_Realize_Fn)(void *ctx_obj, void *thing, void *data);

struct _sie_XML_Iterator {
    sie_Iterator parent;
    sie_XML *node;
    sie_XML *next_xml;
    void *cur;
    sie_XML_Realize_Fn *realize;
    void *data;
    sie_String *name;
    sie_String *attr;
    sie_String *value;
};
SIE_CLASS_DECL(sie_XML_Iterator);
#define SIE_XML_ITERATOR(p) SIE_SAFE_CAST(p, sie_XML_Iterator)

SIE_DECLARE(sie_XML_Iterator *) sie_xml_iterator_new(
    sie_XML *node, sie_String *name, sie_String *attr, sie_String *value,
    sie_XML_Realize_Fn *realize, void *data);
SIE_DECLARE(void) sie_xml_iterator_init(
    sie_XML_Iterator *self, sie_XML *node,
    sie_String *name, sie_String *attr, sie_String *value,
    sie_XML_Realize_Fn *realize, void *data);
SIE_DECLARE(void) sie_xml_iterator_destroy(sie_XML_Iterator *self);
SIE_DECLARE(void *) sie_xml_iterator_iterator_next(sie_XML_Iterator *self);

#endif

#endif
