/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#include "sie_config.h"


#include "sie_iterator.h"

SIE_API_METHOD(sie_iterator_next, void *, NULL, self, (void *self), (self));

SIE_CLASS(sie_Iterator, sie_Context_Object,
          SIE_MDEF(sie_iterator_next, sie_abstract_method)
          SIE_MDEF(sie_destroy, sie_iterator_destroy));

void sie_iterator_init(sie_Iterator *self, void *ctx_obj)
{
    sie_context_object_init(SIE_CONTEXT_OBJECT(self), ctx_obj);
}

void sie_iterator_destroy(sie_Iterator *self)
{
    sie_context_object_destroy(SIE_CONTEXT_OBJECT(self));
}

/*
SIE_VOID_METHOD(sie_iterator_load_current, self, (void *self), (self));
SIE_VOID_METHOD(sie_iterator_release_current, self, (void *self), (self));

          SIE_MDEF(sie_iterator_load_current, sie_abstract_method)
          SIE_MDEF(sie_iterator_release_current, sie_abstract_method)

void *sie_iterator_iter_next(sie_Iterator *self)
{
    sie_iterator_release_current(self);
    sie_iterator_load_current(self);
    return self->current;
}

*/

void sie_vec_iterator_init(sie_Vec_Iterator *self, void *ctx_obj,
                           void *grip, void *vec)
{
    sie_iterator_init(SIE_ITERATOR(self), ctx_obj);
    self->grip = sie_retain(grip);
    self->vec = vec;
    self->index = -1;
}

void *sie_vec_iterator_iterator_next(sie_Vec_Iterator *self)
{
    /* KLUDGE not safe vs. modifications */
    sie_release(self->cur);
    self->cur = NULL;
    self->index++;
    if (self->index < (ssize_t)sie_vec_size(self->vec)) {
        self->cur = self->vec[self->index];
    } else {
        self->cur = NULL;
    }
    return sie_retain(self->cur);
}

void sie_vec_iterator_destroy(sie_Vec_Iterator *self)
{
    sie_release(self->cur);
    sie_release(self->grip);
    sie_iterator_destroy(SIE_ITERATOR(self));
}

SIE_CONTEXT_OBJECT_NEW_FN(
    sie_vec_iterator_new, sie_Vec_Iterator, self, ctx_obj,
    (void *ctx_obj, void *grip, void *vec),
    sie_vec_iterator_init(self, ctx_obj, grip, vec));

SIE_CLASS(sie_Vec_Iterator, sie_Iterator,
          SIE_MDEF(sie_destroy, sie_vec_iterator_destroy)
          SIE_MDEF(sie_iterator_next, sie_vec_iterator_iterator_next));



void sie_id_map_iterator_init(
    sie_Id_Map_Iterator *self, sie_Id_Map *id_map,
    sie_Id_Map_Realize_Fn *realize, void *data)
{
    sie_iterator_init(SIE_ITERATOR(self), id_map);
    self->id_map = sie_retain(id_map);
    self->realize = realize;
    self->data = sie_retain(data);  /* KLUDGE?! */
    self->index = -1;
}

void *sie_id_map_iterator_iterator_next(sie_Id_Map_Iterator *self)
{
    /* KLUDGE share code with sie_id_map_foreach */
    /* KLUDGE not safe vs. modifications */
    /* KLUDGE ugly as sin */
    sie_release(self->cur);
    self->cur = NULL;
    if (!self->overflow) {
        for (;;) {
            self->index++;
            if (self->index < (ssize_t)self->id_map->num_direct) {
                if (self->id_map->direct[self->index]) {
                    self->cur =
                        self->realize(self,
                                      self->id_map->direct[self->index],
                                      self->data);
                    if (self->cur) break;
                }
            } else {
                self->overflow = 1;
                self->index = -1;
                break;
            }
        }
    }
    if (self->overflow) {
        for (;;) {
            self->index++;
            if (self->index < (ssize_t)sie_vec_size(self->id_map->oflow_ids)) {
                if (self->id_map->oflow_ids[self->index]) {
                    self->cur =
                        self->realize(self,
                                      self->id_map->oflow_values[self->index],
                                      self->data);
                    if (self->cur) break;
                }
            } else {
                self->cur = NULL;
                break;
            }
        }
    }
    return self->cur;
}

void sie_id_map_iterator_destroy(sie_Id_Map_Iterator *self)
{
    sie_release(self->cur);
    sie_release(self->data);
    sie_release(self->id_map);
    sie_iterator_destroy(SIE_ITERATOR(self));
}

SIE_CONTEXT_OBJECT_NEW_FN(
    sie_id_map_iterator_new, sie_Id_Map_Iterator, self, id_map,
    (sie_Id_Map *id_map, sie_Id_Map_Realize_Fn *realize, void *data),
    sie_id_map_iterator_init(self, id_map, realize, data));

SIE_CLASS(sie_Id_Map_Iterator, sie_Iterator,
          SIE_MDEF(sie_iterator_next, sie_id_map_iterator_iterator_next)
          SIE_MDEF(sie_destroy, sie_id_map_iterator_destroy));


void sie_xml_iterator_init(sie_XML_Iterator *self, sie_XML *node,
                           sie_String *name, sie_String *attr,
                           sie_String *value, sie_XML_Realize_Fn *realize,
                           void *data)
{
    sie_iterator_init(SIE_ITERATOR(self), node);
    self->node = sie_retain(node);
    self->name = name;
    self->attr = attr;
    self->value = value;
    self->realize = realize;
    self->data = sie_retain(data);  /* KLUDGE?! */
    self->next_xml =
        sie_xml_find_element_s(self->node, self->node,
                               self->name, self->attr, self->value,
                               SIE_XML_DESCEND_ONCE);
}

void *sie_xml_iterator_iterator_next(sie_XML_Iterator *self)
{
    /* KLUDGE not safe vs. modifications */
    sie_release(self->cur);
    self->cur = NULL;
    
    if (self->next_xml) {
        self->cur = self->realize(self, self->next_xml, self->data);
        self->next_xml =
            sie_xml_find_element_s(self->next_xml, self->node,
                                   self->name, self->attr, self->value,
                                   SIE_XML_NO_DESCEND);
    } else {
        self->cur = NULL;
    }

    return self->cur;
}

void sie_xml_iterator_destroy(sie_XML_Iterator *self)
{
    sie_release(self->cur);
    sie_release(self->data);
    sie_release(self->node);
    sie_iterator_destroy(SIE_ITERATOR(self));
}

SIE_CONTEXT_OBJECT_NEW_FN(
    sie_xml_iterator_new, sie_XML_Iterator, self, node,
    (sie_XML *node, sie_String *name, sie_String *attr, sie_String *value,
     sie_XML_Realize_Fn *realize, void *data),
    sie_xml_iterator_init(self, node, name, attr, value, realize, data));

SIE_CLASS(sie_XML_Iterator, sie_Iterator,
          SIE_MDEF(sie_destroy, sie_xml_iterator_destroy)
          SIE_MDEF(sie_iterator_next, sie_xml_iterator_iterator_next));
