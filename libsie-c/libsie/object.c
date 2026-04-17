/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#include "sie_config.h"

#include <stdlib.h>
#include <stdio.h>

#include "sie_object.h"
#include "sie_vec.h"

#if SIE_DEBUG >= 200
#define DD(x) x
#else
#define DD(x)
#endif

/* rest */

void sie_abstract_method(void *self, ...)
{
    fprintf(stderr, "Tried to call an abstract method on instance %p "
            "of class '%s'.\n", self, sie_class_name(self));
    abort();
}

void sie_copy_not_applicable(void *self, ...)
{
    fprintf(stderr, "Copy not applicable to instance %p of class '%s'.\n",
            self, sie_object_class_name(self));
    abort();
}

sie_Class *sie_class_of(void *p)
{
    if (p)
        return SIE_CLASS_OF(p);
    else
        return NULL;
}

sie_Class *sie_class_parent(sie_Class *p)
{
    return p->parent;
}

const char *sie_class_name(sie_Class *p)
{
    return p->name;
}

static sie_Methods *class_init(sie_Class *class_, sie_Class *cur)
{
    void *self = NULL;  /* make vec happy, will abort on error */
    sie_Methods *methods;
    sie_Methods *method;
    int i;
    if (cur->resolve_parent_fn) {
        cur->parent = cur->resolve_parent_fn();
        cur->resolve_parent_fn = NULL;
    }
    if (cur->parent) {
        methods = class_init(class_, cur->parent);
    } else {
        _sie_vec_init(methods, 0);
    }
    for (i = 0; i < cur->num_methods; i++) {
        int found = 0;
        if (cur->methods[i].resolve_method_fn) {
            cur->methods[i].method = cur->methods[i].resolve_method_fn();
            cur->methods[i].resolve_method_fn = NULL;
        }
        if (cur->parent) {
            sie_vec_forall(methods, method) {
                if (method->method == cur->methods[i].method) {
                    method->implementation =
                        cur->methods[i].implementation;
                    found = 1;
                }
            }
        }
        if (!found)
            sie_vec_push_back(methods, cur->methods[i]);
    }
    if (class_ == cur) {
        class_->num_all_methods = (int)sie_vec_size(methods);
        class_->all_methods = malloc(sizeof(*methods) * sie_vec_size(methods));
        memcpy(class_->all_methods, methods,
               sizeof(*methods) * sie_vec_size(methods));
        sie_vec_free(methods);
    }
    return methods;
}

void *sie_class_check_cast(void *p, sie_Class *to_class)
{
    sie_Class *from_class = SIE_CLASS_OF(p);
    sie_Class *cur;
    
    for (cur = from_class; cur != NULL; cur = cur->parent)
        if (cur == to_class) return p;

    fprintf(stderr, "Tried a bad cast from %s to %s.\n",
            from_class->name, to_class->name);
    abort();

    return NULL;
}

int sie_class_lookup_method(sie_Class *class_, sie_Generic_Method *method)
{
    int i;
    for (i = 0; i < class_->num_all_methods; i++) {
        if (class_->all_methods[i].method == method)
            return i;
    }
    fprintf(stderr, "Failed to look up method %p in class %s.\n",
            (void *)method, class_->name);
    abort();
}

/* *** sie_Base_Object ********** */

void sie_alloc_in_place(sie_Class *class_, void *place)
{
    sie_Base_Object *self = place;
    if (!class_->all_methods)
        class_init(class_, class_);
    self->class_ = class_;
}

void *sie_alloc(sie_Class *class_)
{
    sie_Base_Object *self = calloc(1, class_->size);
    if (!self)
        abort();
    if (!class_->all_methods)
        class_init(class_, class_);
    self->class_ = class_;
    return self;
}

void *sie_base_object_init(sie_Base_Object *self)
{
    return self;
}

SIE_VOID_METHOD(sie_destroy, self, (void *self), (self));

void sie_base_object_destroy(sie_Base_Object *self) { }

SIE_METHOD(sie_copy, void *, self, (void *self), (self));

void *sie_base_object_copy(sie_Base_Object *self)
{
    sie_Base_Object *copy = sie_alloc(SIE_CLASS_OF(self));
    memcpy(copy, self, SIE_CLASS_OF(self)->size);
    return copy;
}

SIE_VOID_METHOD(sie_free_object, self, (void *self), (self));

void sie_base_object_free_object(sie_Base_Object *self)
{
    free(self);
}

SIE_RAW_CLASS(sie_Base_Object, NULL,
              SIE_MDEF(sie_destroy, sie_base_object_destroy)
              SIE_MDEF(sie_copy, sie_base_object_copy)
              SIE_MDEF(sie_free_object, sie_base_object_free_object));

/* *** sie_Refcounted_Object ********** */

void *sie_refcounted_object_init(sie_Refcounted_Object *self)
{
    void *result = sie_base_object_init(SIE_BASE_OBJECT(self));
    self->refcount = 1;
    return result;
}

void sie_refcounted_object_destroy(sie_Refcounted_Object *self)
{
    sie_base_object_destroy(SIE_BASE_OBJECT(self));
}

void *sie_refcounted_object_copy(sie_Refcounted_Object *self)
{
    sie_Refcounted_Object *copy =
        sie_base_object_copy(SIE_BASE_OBJECT(self));
    copy->refcount = 1;
    return copy;
}

/* See below for sie_retain */

void sie_release(void *v_self)
{
    if (v_self) {
        sie_Refcounted_Object *self = SIE_REFCOUNTED_OBJECT(v_self);
        if (self->refcount == 0) abort();
        if (--self->refcount <= 0) {
            sie_destroy(self);
            sie_free_object(self);
        }
    }
}

/*
void sie_release(void *v_self)
{
    if (v_self)
        _sie_release(v_self);
}

SIE_VOID_METHOD(_sie_release, self, (void *self), (self));

void sie_refcounted_object_release(sie_Refcounted_Object *self)
{
    if (--self->refcount <= 0) {
        sie_destroy(self);
        sie_free_object(self);
    }
}
*/

SIE_CLASS(sie_Refcounted_Object, sie_Base_Object,
          SIE_MDEF(sie_destroy, sie_refcounted_object_destroy)
/*          SIE_MDEF(_sie_release, sie_refcounted_object_release) */
          SIE_MDEF(sie_copy, sie_refcounted_object_copy));

/* *** sie_Weak_Ref ********** */

SIE_OBJECT_NEW_FN(sie_weak_ref_new, sie_Weak_Ref, self,
                  (sie_Object *target),
                  sie_weak_ref_init(self, target));

void *sie_weak_ref_init(sie_Weak_Ref *self, sie_Object *target)
{
    void *result = sie_refcounted_object_init(SIE_REFCOUNTED_OBJECT(self));
    self->target = target;
    return result;
}

SIE_VOID_METHOD(sie_break_weak_ref, self, (void *self), (self));

void sie_weak_ref_break_weak_ref(sie_Weak_Ref *self)
{
    self->target = NULL;
}

void sie_weak_ref_destroy(sie_Weak_Ref *self)
{
    sie_refcounted_object_destroy(SIE_REFCOUNTED_OBJECT(self));
}

void *sie_weak_deref(void *self)
{
    if (self)
        return SIE_WEAK_REF(self)->target;
    else
        return NULL;
}

SIE_CLASS(sie_Weak_Ref, sie_Refcounted_Object,
          SIE_MDEF(sie_destroy, sie_weak_ref_destroy)
          SIE_MDEF(sie_copy, sie_copy_not_applicable)
          SIE_MDEF(sie_break_weak_ref, sie_weak_ref_break_weak_ref));

sie_Weak_Ref *sie_get_weak_ref(void *v_self)
{
    sie_Object *self = SIE_OBJECT(v_self);
    if (!self->weak_ref)
        self->weak_ref = sie_weak_ref_new(self);
    sie_retain(self->weak_ref);
    return self->weak_ref;
}

/* *** sie_Object ********** */

void *sie_object_init(sie_Object *self)
{
    return sie_refcounted_object_init(SIE_REFCOUNTED_OBJECT(self));
}

void sie_object_destroy(sie_Object *self)
{
     if (self->weak_ref) {
        sie_break_weak_ref(self->weak_ref);
        sie_release(self->weak_ref);
    }
    sie_refcounted_object_destroy(SIE_REFCOUNTED_OBJECT(self));
}

void *sie_object_copy(sie_Object *self)
{
    sie_Object *copy =
        sie_refcounted_object_copy(SIE_REFCOUNTED_OBJECT(self));
    copy->weak_ref = NULL;
    return copy;
}

SIE_CLASS(sie_Object, sie_Refcounted_Object,
          SIE_MDEF(sie_destroy, sie_object_destroy)
          SIE_MDEF(sie_copy, sie_object_copy));

#undef sie_retain
void *sie_retain(void *v_self)
{
    if (v_self)
        sie_refcount(v_self)++;
    return v_self;
}
