/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#include "sie_config.h"

#include <stdlib.h>
#include <string.h>

#include "sie_id_map.h"

extern void sie_id_map_foreach_release(sie_id id, void *object, void *extra)
{
    sie_release(object);
}

SIE_CONTEXT_OBJECT_NEW_FN(sie_id_map_new, sie_Id_Map, self, ctxobj,
                          (void *ctxobj, size_t initial),
                          sie_id_map_init(self, ctxobj, initial));

void sie_id_map_init(sie_Id_Map *self, void *ctxobj, size_t initial)
{
    sie_context_object_init(SIE_CONTEXT_OBJECT(self), ctxobj);
    self->num_direct = initial;
    self->max_id = SIE_NULL_ID;
    if (initial)
        self->direct = sie_calloc(self, initial * sizeof(*self->direct));
    _sie_vec_init(self->oflow_ids, 0);
    _sie_vec_init(self->oflow_values, 0);
}

void sie_id_map_destroy(sie_Id_Map *self)
{
    sie_vec_free(self->oflow_ids);
    sie_vec_free(self->oflow_values);
    free(self->direct);
    sie_context_object_destroy(SIE_CONTEXT_OBJECT(self));
}

SIE_CLASS(sie_Id_Map, sie_Context_Object,
          SIE_MDEF(sie_destroy, sie_id_map_destroy));

static void id_map_grow(sie_Id_Map *self)
{
    do {
        size_t new_num = self->num_direct * 2;
        size_t i;
        sie_id *new_oflow_ids = NULL;
        void **new_oflow_values = NULL;

        if (new_num == 0) new_num = 16;  /* KLUDGE some value */
        
        self->direct = sie_realloc(self, self->direct,
                                   new_num * sizeof(*self->direct));
        memset(&self->direct[self->num_direct], 0,
               (new_num - self->num_direct) * sizeof(*self->direct));
        self->num_direct = new_num;
        
        for (i = 0; i < sie_vec_size(self->oflow_ids); i++) {
            sie_id id = self->oflow_ids[i];
            void *value = self->oflow_values[i];
            if (id < self->num_direct) {
                self->direct[id] = value;
            } else {
                sie_vec_push_back(new_oflow_ids, id);
                sie_vec_push_back(new_oflow_values, value);
            }
        }
        
        sie_vec_free(self->oflow_ids);
        sie_vec_overwrite(self->oflow_ids, new_oflow_ids);
        sie_vec_free(self->oflow_values);
        sie_vec_overwrite(self->oflow_values, new_oflow_values);
        
        /* Avoid getting locked into a pessimistic allocation pattern
         * - if the top direct element is filled, grow again.  KLUDGE
         * - is this correct?  It's possible to make it grow very
         * large with the right inputs. */
    } while (self->direct[self->num_direct - 1]);
}

void sie_id_map_foreach(sie_Id_Map *self, sie_Id_Map_Foreach_Fn *fn,
                        void *extra)
{
    size_t i;
    if (!self) return;
    for (i = 0; i < self->num_direct; i++)
        if (self->direct[i])
            fn((sie_id)i, self->direct[i], extra);
    for (i = 0; i < sie_vec_size(self->oflow_ids); i++)
        if (self->oflow_values[i])
            fn(self->oflow_ids[i], self->oflow_values[i], extra);
}

void *sie_id_map_get(sie_Id_Map *self, sie_id id)
{
    size_t i;
    if (id < self->num_direct)
        return self->direct[id];
    for (i = 0; i < sie_vec_size(self->oflow_ids); i++)
        if (self->oflow_ids[i] == id)
            return self->oflow_values[i];
    return NULL;
}

void sie_id_map_set(sie_Id_Map *self, sie_id id, void *value)
{
    size_t i;
    if (self->max_id == SIE_NULL_ID || id > self->max_id)
        self->max_id = id;
    if (id == self->num_direct)
        id_map_grow(self);
    if (id < self->num_direct) {
        self->direct[id] = value;
        return;
    }
    for (i = 0; i < sie_vec_size(self->oflow_ids); i++) {
        if (self->oflow_ids[i] == id) {
            self->oflow_values[i] = value;
            return;
        }
    }
    sie_vec_push_back(self->oflow_ids, id);
    sie_vec_push_back(self->oflow_values, value);
}

sie_id sie_id_map_get_max_id(sie_Id_Map *self)
{
    return self->max_id;
}
