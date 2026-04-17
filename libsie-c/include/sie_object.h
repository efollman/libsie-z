/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

typedef struct _sie_Class sie_Class;

typedef struct _sie_Methods sie_Methods;
typedef struct _sie_Base_Object sie_Base_Object;
typedef struct _sie_Refcounted_Object sie_Refcounted_Object;
typedef struct _sie_Object sie_Object;
typedef struct _sie_Weak_Ref sie_Weak_Ref;

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_OBJECT_H
#define SIE_OBJECT_H

#include <stdlib.h>

#define SODBG(x) 

typedef void *(sie_Generic_Method)(void *, ...);

/* KLUDGE - needed for DLL loading */
typedef sie_Class *(sie_Resolve_Parent_Fn)(void);
typedef sie_Generic_Method *(sie_Resolve_Method_Fn)(void);

struct _sie_Methods {
    char *name;
    sie_Generic_Method *method;
    sie_Resolve_Method_Fn *resolve_method_fn;
    sie_Generic_Method *implementation;
};

struct _sie_Class {
    sie_Class *parent;
    sie_Resolve_Parent_Fn *resolve_parent_fn;
    char *name;
    size_t size;
    sie_Methods *methods;
    int num_methods;
    /* guts */
    sie_Methods *all_methods;
    int num_all_methods;
};

#define SIE_MDEF(method, implementation)         \
    { #method, NULL, (method##_RESOLVE_METHOD),  \
      (sie_Generic_Method *)(implementation) },

#define SIE_CLASS_NAME_FOR_TYPE(type) type##_CLASS
#define SIE_CLASS_FOR_TYPE(type) (&SIE_CLASS_NAME_FOR_TYPE(type))
#define SIE_CLASS_DECL(type) \
    extern SIE_DECLARE_DATA sie_Class SIE_CLASS_NAME_FOR_TYPE(type)

#define SIE_RAW_CLASS(type, parent, methods)                    \
    static sie_Methods type##_METHODS[] = { methods };          \
    sie_Class SIE_CLASS_NAME_FOR_TYPE(type) = {                 \
        parent, NULL, #type, sizeof(type), type##_METHODS,      \
        (sizeof(type##_METHODS) / sizeof(*(type##_METHODS))),   \
        NULL, 0                                                 \
    }

/* KLUDGE - duplicated because of CPP comma details */
#define SIE_CLASS(type, parent_type, methods)                   \
    static sie_Class *type##_RESOLVE_PARENT(void)               \
    { return SIE_CLASS_FOR_TYPE(parent_type); }                 \
    static sie_Methods type##_METHODS[] = { methods };          \
    sie_Class SIE_CLASS_NAME_FOR_TYPE(type) = {                 \
        NULL, type##_RESOLVE_PARENT, #type, sizeof(type),       \
        type##_METHODS,                                         \
        (sizeof(type##_METHODS) / sizeof(*(type##_METHODS))),   \
        NULL, 0                                                 \
    }

#if SIE_DEBUG > 0
#define SIE_SAFE_CAST(p, type)                                \
    ((type *)sie_class_check_cast(p, SIE_CLASS_FOR_TYPE(type)))
#else
#define SIE_SAFE_CAST(p, type) ((type *)(p))
#endif

SIE_DECLARE(void *) sie_class_check_cast(void *p, sie_Class *to_class);

/* *** sie_Base_Object ********** */

struct _sie_Base_Object {
    sie_Class *class_;
};
SIE_CLASS_DECL(sie_Base_Object);
#define SIE_BASE_OBJECT(p) SIE_SAFE_CAST(p, sie_Base_Object)

#define SIE_CLASS_OF(p) (((sie_Base_Object *)p)->class_)
SIE_DECLARE(sie_Class *) sie_class_of(void *p);
SIE_DECLARE(sie_Class *) sie_class_parent(sie_Class *p);
SIE_DECLARE(const char *) sie_class_name(sie_Class *p);
#define sie_object_class_name(p) (SIE_CLASS_OF(p)->name)
SIE_DECLARE(int) sie_class_lookup_method(
    sie_Class *class_, sie_Generic_Method *method);
SIE_DECLARE_NONSTD(void) sie_abstract_method(void *self, ...);
SIE_DECLARE_NONSTD(void) sie_copy_not_applicable(void *self, ...);

#define SIE_METHOD_DECL(mname) \
    SIE_DECLARE(sie_Generic_Method *) mname##_RESOLVE_METHOD(void)

#define SIE_RAW_METHOD(mname, pre, maybe_return, ret_type, object_arg,  \
                       signature, call_args, va_decl, va_start, va_end) \
    ret_type mname signature                                            \
    {                                                                   \
        pre;                                                            \
        {                                                               \
            static int method_offset = -1;                              \
            sie_Class *class_ = SIE_BASE_OBJECT(object_arg)->class_;    \
            va_decl;                                                    \
            if (method_offset == -1)                                    \
                method_offset =                                         \
                    sie_class_lookup_method(                            \
                        class_, (sie_Generic_Method *)mname);           \
            if ((method_offset > class_->num_all_methods) ||            \
                (class_->all_methods[method_offset].method !=           \
                 (sie_Generic_Method *)mname))                          \
                abort();                                                \
            SODBG(fprintf(stderr, "Calling method '%s' on instance %p " \
                          "of class '%s'.\n", #mname, object_arg,       \
                          sie_class_name(object_arg)));                 \
            va_start;                                                   \
            maybe_return                                                \
                ((ret_type (*) signature)                               \
                 class_->all_methods[method_offset].implementation)     \
                call_args;                                              \
            va_end;                                                     \
        }                                                               \
    }                                                                   \
    sie_Generic_Method *mname##_RESOLVE_METHOD(void)                    \
    { return (sie_Generic_Method *)mname; }

#define SIE_VOID_METHOD(name, object_arg, signature, call_args)         \
    SIE_RAW_METHOD(name, ;, ;, void, object_arg, signature, call_args, ;, ;, ;)

#define SIE_METHOD(name, ret_type, object_arg, signature, call_args)    \
    SIE_RAW_METHOD(name, ;, return, ret_type, object_arg, signature,    \
                   call_args, ;, ;, ;)

#define SIE_VOID_VARARGS_METHOD(name, object_arg, va_arg, last,         \
                                signature, call_args)                   \
    SIE_RAW_METHOD(name, ;, ;, void, object_arg, signature, call_args,  \
                   va_list va_arg, va_start(va_arg, last), va_end(va_arg))

#define SIE_VARARGS_METHOD(name, ret_type, object_arg, va_arg, last,    \
                           signature, call_args)                        \
    SIE_RAW_METHOD(name, ;, retval =, ret_type, object_arg,             \
                   signature, call_args,                                \
                   va_list va_arg; ret_type retval,                     \
                   va_start(va_arg, last), va_end(va_arg); return retval)

#define SIE_OBJECT_NEW_FN(name, type, self, arg_list, call)     \
    type *name arg_list                                         \
    {                                                           \
        SIE_ALLOC(self, type);                                  \
        if (self) {                                             \
            if (call)                                           \
                return self;                                    \
            sie_destroy(self);                                  \
            sie_free_object(self);                              \
        }                                                       \
        return NULL;                                            \
    }

#define SIE_OBJECT_VARARGS_NEW_FN(name, type, self, va_arg, last,       \
                                  arg_list, call)                       \
    type *name arg_list                                                 \
    {                                                                   \
        SIE_ALLOC(self, type);                                          \
        va_list va_arg;                                                 \
        void *_value;                                                   \
        if (self) {                                                     \
            va_start(va_arg, last);                                     \
            _value = call;                                              \
            va_end(va_arg);                                             \
            if (_value)                                                 \
                return self;                                            \
            sie_destroy(self);                                          \
            sie_free_object(self);                                      \
        }                                                               \
        return NULL;                                                    \
    }

SIE_DECLARE(void *) sie_alloc(sie_Class *class_);
SIE_DECLARE(void) sie_alloc_in_place(sie_Class *class_, void *place);

#define SIE_ALLOC(self, type)                           \
    type *self = (type *)sie_alloc(SIE_CLASS_FOR_TYPE(type))

SIE_DECLARE(void *) sie_base_object_init(sie_Base_Object *self);

SIE_METHOD_DECL(sie_destroy);
SIE_DECLARE(void) sie_destroy(void *self);
SIE_DECLARE(void) sie_base_object_destroy(sie_Base_Object *self);

SIE_METHOD_DECL(sie_copy);
SIE_DECLARE(void *) sie_copy(void *self);
SIE_DECLARE(void *) sie_base_object_copy(sie_Base_Object *self);

SIE_METHOD_DECL(sie_free_object);
SIE_DECLARE(void) sie_free_object(void *v_self);
SIE_DECLARE(void) sie_base_object_free_object(sie_Base_Object *self);

/* *** sie_Refcounted_Object ********** */

struct _sie_Refcounted_Object {
    sie_Base_Object parent;
    int refcount;
};
SIE_CLASS_DECL(sie_Refcounted_Object);
#define SIE_REFCOUNTED_OBJECT(p) SIE_SAFE_CAST(p, sie_Refcounted_Object)

#define sie_refcount(p) (SIE_REFCOUNTED_OBJECT(p)->refcount)

SIE_DECLARE(void *) sie_refcounted_object_init(sie_Refcounted_Object *self);
SIE_DECLARE(void *) sie_refcounted_object_copy(sie_Refcounted_Object *self);
SIE_DECLARE(void) sie_refcounted_object_destroy(sie_Refcounted_Object *self);

SIE_DECLARE(void *) sie_retain(void *v_self);
#if !defined(SIE_DEBUG) || SIE_DEBUG > 0
#define sie_retain(v) ((v) ? (sie_refcount(v)++, (v)) : NULL)
#endif
SIE_DECLARE(void) sie_release(void *v_self);
SIE_METHOD_DECL(_sie_release);
SIE_DECLARE(void) _sie_release(void *self);

SIE_DECLARE(void) sie_refcounted_object_release(sie_Refcounted_Object *self);

/* *** sie_Weak_Ref ********** */

struct _sie_Weak_Ref {
    sie_Refcounted_Object parent;
    sie_Object *target;
};
SIE_CLASS_DECL(sie_Weak_Ref);
#define SIE_WEAK_REF(p) SIE_SAFE_CAST(p, sie_Weak_Ref)

SIE_DECLARE(sie_Weak_Ref *) sie_weak_ref_new(sie_Object *target);
SIE_DECLARE(void *) sie_weak_ref_init(sie_Weak_Ref *self, sie_Object *target);

SIE_METHOD_DECL(sie_break_weak_ref);
SIE_DECLARE(void) sie_break_weak_ref(void *self);
SIE_DECLARE(void) sie_weak_ref_break_weak_ref(sie_Weak_Ref *self);

SIE_DECLARE(void *) sie_weak_deref(void *self);
SIE_DECLARE(void) sie_weak_ref_destroy(sie_Weak_Ref *self);

/* *** sie_Object ********** */

struct _sie_Object {
    sie_Refcounted_Object parent;
    sie_Weak_Ref *weak_ref;
};
SIE_CLASS_DECL(sie_Object);
#define SIE_OBJECT(p) SIE_SAFE_CAST(p, sie_Object)

SIE_DECLARE(void *) sie_object_init(sie_Object *self);

SIE_DECLARE(sie_Weak_Ref *) sie_get_weak_ref(void *v_self);

SIE_DECLARE(void *) sie_object_copy(sie_Object *self);
SIE_DECLARE(void) sie_object_destroy(sie_Object *self);

#endif

#endif
