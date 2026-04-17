/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

typedef struct _sie_Context sie_Context;
typedef struct _sie_Context_Object sie_Context_Object;
typedef struct _sie_Error_Context sie_Error_Context;

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_CONTEXT_H
#define SIE_CONTEXT_H

#include <stdio.h>
#include <setjmp.h>

struct _sie_Context_Object {
    sie_Object parent;
    sie_Context *context;
};
SIE_CLASS_DECL(sie_Context_Object);
#define SIE_CONTEXT_OBJECT(p) SIE_SAFE_CAST(p, sie_Context_Object)

SIE_DECLARE(void) sie_context_object_init(sie_Context_Object *self, 
                                          void *other);
SIE_DECLARE(void *) sie_context_object_copy(sie_Context_Object *self);
SIE_DECLARE(sie_Context *) sie_context(void *v_ctx_ob);
SIE_DECLARE(void) sie_context_object_destroy(sie_Context_Object *self);

#if SIE_DEBUG >= 0
#include <stdio.h>
#endif

typedef void (sie_Cleanup_Fn)(void *);
typedef void (sie_Exception_Callback)(sie_Exception *ex, void *data);
typedef int (sie_Progress_Set_Message)(void *data, const char *message);
typedef int (sie_Progress_Count)(void *data, sie_uint64 done, sie_uint64 total);
typedef int (sie_Progress_Percent)(void *data, sie_float64 percent_done);

#ifdef SIE_LEAK_CHECK
#include "sie_uthash.h"

struct sie_alloc_record {
    sie_Context_Object *object;
    int refcount;
    UT_hash_handle hh;
};
#endif

struct _sie_Context {
    sie_Context_Object parent;
    apr_status_t apr_initialize_result;
    apr_pool_t *pool;
    sie_Handler *top_handler;
    sie_Exception *exception;
    int num_inits;
    int num_destroys;
    int *num_inits_p;
    int *num_destroys_p;
    sie_Cleanup_Fn **cleanup_fns;
    void **cleanup_targets;
    sie_Error_Context *error_context_top;
    char *oom_buffer;
    FILE *debug_stream;
    int debug_level;
    sie_Exception_Callback *unhandled_exception_callback;
    void *unhandled_exception_data;
    sie_Exception_Callback *api_exception_callback;
    void *api_exception_data;
    size_t ignore_trailing_garbage;
    int progress_enabled;
    void *progress_data;
    sie_Progress_Set_Message *progress_set_message;
    sie_Progress_Count *progress_count;
    sie_Progress_Percent *progress_percent;
    int progress_percent_last;
    sie_String_Table *string_table;
    sie_String **string_literals;
    int recursion_count;
#ifdef SIE_LEAK_CHECK
    struct sie_alloc_record *allocations;
#endif
};
SIE_CLASS_DECL(sie_Context);
#define SIE_CONTEXT(p) SIE_SAFE_CAST(p, sie_Context)

#define sie_string_table(ctx_obj) (sie_context(ctx_obj)->string_table)

SIE_DECLARE(sie_Context *) sie_context_new(void);
SIE_DECLARE(void *) sie_context_init(sie_Context *self);
SIE_DECLARE(void) sie_context_destroy(sie_Context *self);

SIE_DECLARE(void) sie_set_unhandled_exception_callback(
    void *ctx_obj, sie_Exception_Callback *cb, void *data);
SIE_DECLARE(void) sie_set_api_exception_callback(
    void *ctx_obj, sie_Exception_Callback *cb, void *data);
SIE_DECLARE(sie_Exception *) sie_check_exception(void *ctx_obj);
SIE_DECLARE(sie_Exception *) sie_get_exception(void *ctx_obj);
SIE_DECLARE(int) sie_context_done(sie_Context *self);

SIE_DECLARE(size_t) sie_cleanup_mark(void *ctx_obj);
SIE_DECLARE(void *) sie_cleanup_push(void *ctx_obj, 
                                     sie_Cleanup_Fn *fn, void *target);
SIE_DECLARE(void) sie_cleanup_pop(void *ctx_obj, void *target, int fire);
SIE_DECLARE(void) sie_cleanup_pop_unchecked(void *ctx_obj, int fire);
SIE_DECLARE(void) sie_cleanup_pop_mark(void *ctx_obj, size_t mark);

SIE_DECLARE(void *) sie_autorelease(void *object);

SIE_DECLARE(void *) sie_malloc(void *ctx_obj, size_t size);
SIE_DECLARE(void *) sie_calloc(void *ctx_obj, size_t size);
SIE_DECLARE(void *) sie_realloc(void *ctx_obj, void *ptr, size_t size);
SIE_DECLARE(void *) sie_memdup(void *ctx_obj, const void *data,
                               size_t size);
SIE_DECLARE(char *) sie_strdup(void *ctx_obj, const char *string);

SIE_DECLARE(void) sie_ignore_trailing_garbage(void *ctx_obj, size_t amount);

SIE_DECLARE(void) sie_recursion_limit(void *ctx_obj);

SIE_DECLARE(void *) sie_ctx_alloc(sie_Class *class_, void *ctx_obj);
#define SIE_CTX_ALLOC(self, type, ctx_obj)                              \
    type *self = (type *)sie_ctx_alloc(SIE_CLASS_FOR_TYPE(type), ctx_obj)

#define SIE_CONTEXT_OBJECT_NEW_FN(name, type, self, ctx_obj,            \
                                  arg_list, call)                       \
    type *name arg_list                                                 \
    {                                                                   \
        SIE_CTX_ALLOC(self, type, ctx_obj);                             \
        if (self) {                                                     \
            SIE_TRY(ctx_obj) {                                          \
                call;                                                   \
            } SIE_CATCH(_e) {                                           \
                (void)_e;                                               \
                sie_destroy(self);                                      \
                sie_free_object(self);                                  \
                SIE_RETHROW();                                          \
            } SIE_NO_FINALLY();                                         \
            return self;                                                \
        }                                                               \
        sie_throw_oom(ctx_obj);                                         \
        return NULL; /* not reached */                                  \
    }

#define SIE_CONTEXT_OBJECT_API_NEW_FN(name, type, self, ctx_obj,        \
                                      arg_list, call)                   \
    type *name arg_list                                                 \
    {                                                                   \
        if (ctx_obj) {                                                  \
            SIE_CTX_ALLOC(self, type, ctx_obj);                         \
            int volatile _caught = 0;                                   \
            SIE_API_TRY(ctx_obj) {                                      \
                if (self) {                                             \
                    SIE_TRY(ctx_obj) {                                  \
                        call;                                           \
                    } SIE_CATCH(_e) {                                   \
                        (void)_e;                                       \
                        sie_destroy(self);                              \
                        sie_free_object(self);                          \
                        SIE_RETHROW();                                  \
                    } SIE_NO_FINALLY();                                 \
                } else {                                                \
                    sie_throw_oom(ctx_obj);                             \
                }                                                       \
            } SIE_END_API_TRY(_caught);                                 \
            if (_caught)                                                \
                return NULL;                                            \
            return self;                                                \
        } else {                                                        \
            return NULL;                                                \
        }                                                               \
    }

#define SIE_CONTEXT_OBJECT_VARARGS_NEW_FN(name, type, self, ctx_obj,    \
                                          va_arg, last, arg_list, call) \
    type *name arg_list                                                 \
    {                                                                   \
        SIE_CTX_ALLOC(self, type, ctx_obj);                             \
        va_list va_arg;                                                 \
        if (self) {                                                     \
            SIE_TRY(ctx_obj) {                                          \
                va_start(va_arg, last);                                 \
                call;                                                   \
            } SIE_CATCH(_e) {                                           \
                (void)_e;                                               \
                sie_destroy(self);                                      \
                sie_free_object(self);                                  \
                SIE_RETHROW();                                          \
            } SIE_FINALLY() {                                           \
                va_end(va_arg);                                         \
            } SIE_END_FINALLY();                                        \
            return self;                                                \
        }                                                               \
        sie_throw_oom(ctx_obj);                                         \
        return NULL; /* not reached */                                  \
    }

#define SIE_API_METHOD(name, ret_type, onerr_retval, object_arg,        \
                       signature, call_args)                            \
    SIE_RAW_METHOD(name, if (!object_arg) return onerr_retval,          \
                   retval =, ret_type, object_arg,                      \
                   signature, call_args, int volatile _caught = 0;      \
                   ret_type volatile retval = onerr_retval,             \
                   SIE_API_TRY(object_arg) {,                           \
                   } SIE_END_API_TRY(_caught);                          \
                   if (_caught) retval = onerr_retval; return retval)

#define SIE_VOID_API_METHOD(name, object_arg, signature, call_args)     \
    SIE_RAW_METHOD(name, if (!object_arg) return, ;, void, object_arg,  \
                   signature, call_args, int volatile _caught = 0,      \
                   (void)_caught; SIE_API_TRY(object_arg) {,            \
                   } SIE_END_API_TRY(_caught))

struct _sie_Error_Context {
    sie_Context_Object parent;
    char *message;
    sie_Error_Context *next;
};
SIE_CLASS_DECL(sie_Error_Context);

SIE_DECLARE(sie_Error_Context *) sie_error_context_new(
    void *ctx_obj, sie_Error_Context *next, const char *format, va_list args);
SIE_DECLARE(void) sie_error_context_init(
    sie_Error_Context *self, void *ctx_obj, sie_Error_Context *next,
    const char *format, va_list args);
SIE_DECLARE(void) sie_error_context_destroy(sie_Error_Context *self);

SIE_DECLARE(void) sie_error_context_vpush(void *ctx_obj,
                                          const char *format, va_list args);
SIE_DECLARE_NONSTD(void) sie_error_context_push(void *ctx_obj,
                                                const char *format, ...)
    __gcc_attribute__ ((format(printf, 2, 3)));
SIE_DECLARE_NONSTD(void) sie_error_context_auto(void *ctx_obj,
                                                const char *format, ...)
    __gcc_attribute__ ((format(printf, 2, 3)));
SIE_DECLARE(void) sie_error_context_pop(void *ctx_obj);

SIE_DECLARE(void) sie_set_progress_callbacks(
    void *ctx_obj, void *data, 
    sie_Progress_Set_Message *set_message_callback, 
    sie_Progress_Percent *percent_callback);

SIE_DECLARE(void) _sie_progress_msg(sie_Context *ctx, const char *msg);
SIE_DECLARE(void) _sie_progress(sie_Context *ctx,
                                sie_uint64 done, sie_uint64 total);

#define sie_progress_msg(ctx_obj, msg)                  \
    do {                                                \
        sie_Context *_ctx = sie_context(ctx_obj);       \
        if (_ctx && _ctx->progress_enabled)             \
            _sie_progress_msg(_ctx, msg);               \
    } while (0)

#define sie_progress(ctx_obj, done, total)              \
    do {                                                \
        sie_Context *_ctx = sie_context(ctx_obj);       \
        if (_ctx && _ctx->progress_enabled)             \
            _sie_progress(_ctx, done, total);           \
    } while (0)

#endif

#endif
