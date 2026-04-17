/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#define HASH_FUNCTION HASH_FNV

#include "sie_config.h"

#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <stdarg.h>

#include "sie_apr.h"
#include "sie_debug.h"
#include "sie_context.h"
#include "sie_vec.h"

#ifdef SIE_LEAK_CHECK
static struct sie_alloc_record *adjust_alloc_record(
    sie_Context *ctx, void *ptr, int refcount_delta)
{
    struct sie_alloc_record *found;
    HASH_FIND(hh, ctx->allocations, &ptr, sizeof(ptr), found);
    if (!found) {
        found = calloc(1, sizeof(*found));
        found->object = ptr;
        found->refcount = 0;
        HASH_ADD_KEYPTR(hh, ctx->allocations, &found->object,
                        sizeof(found->object), found);
    }
    found->refcount += refcount_delta;
    sie_assert(found->refcount >= 0, ctx);
    if (found->refcount == 0) {
        HASH_DELETE(hh, ctx->allocations, found);
    }
    return found;
}
#else
#define adjust_alloc_record(ctx, ptr, refcount_delta)
#endif

void sie_context_object_init(sie_Context_Object *self, void *other)
{
    sie_object_init(SIE_OBJECT(self));
    if (other) {
        self->context = SIE_CONTEXT_OBJECT(other)->context;
        sie_retain(self->context);
    }
    if (self->context) {
        sie_debug((self, 15, "init called on object %p of type '%s'.\n",
                   self, sie_object_class_name(self)));
        self->context->num_inits++;
        if (self->context->num_inits_p)
            (*self->context->num_inits_p)++;
        adjust_alloc_record(self->context, self, 1);
    }
}

void *sie_context_object_copy(sie_Context_Object *self)
{
    sie_Context_Object *copy = sie_object_copy(SIE_OBJECT(self));
    sie_debug((self, 15, "copy called on object %p of type '%s' -> %p.\n",
               self, sie_object_class_name(self), copy));
    sie_retain(copy->context);
    copy->context->num_inits++;
    if (copy->context->num_inits_p)
        (*copy->context->num_inits_p)++;
    adjust_alloc_record(copy->context, copy, 1);
    return copy;
}

sie_Context *sie_context(void *v_ctx_ob)
{
    sie_Context_Object *ctx_ob = SIE_CONTEXT_OBJECT(v_ctx_ob);
    return ctx_ob->context;
}

void sie_context_object_destroy(sie_Context_Object *self)
{
    if (self->context) {
        adjust_alloc_record(self->context, self, -1);
        sie_debug((self, 15, "destroy called on object %p of type '%s'.\n",
                   self, sie_object_class_name(self)));
        self->context->num_destroys++;
        if (self->context->num_destroys_p)
            (*self->context->num_destroys_p)++;
    }
    sie_release(self->context);
    sie_object_destroy(SIE_OBJECT(self));
}

SIE_CLASS(sie_Context_Object, sie_Object,
          SIE_MDEF(sie_copy, sie_context_object_copy)
          SIE_MDEF(sie_destroy, sie_context_object_destroy));

void *sie_context_init(sie_Context *self)
{
    void *result = self;
    sie_context_object_init(SIE_CONTEXT_OBJECT(self), NULL);
    /* Note: The above is safe with NULL, see above. */
/*     self->parent.ctx_ref = sie_get_weak_ref(self); */
/*     /\* Since we're getting it for ourselves we already have a */
/*      * reference:  release the other one. *\/ */
/*     sie_release(self->parent.ctx_ref); */
/*     /\* Now we can initialize other context objects. *\/ */
    self->parent.context = self;
    /* Now we can initialize other context objects. */

    adjust_alloc_record(self, self, 1);

    self->num_inits = 1;

    if (result && ((self->apr_initialize_result =
                    apr_initialize()) != APR_SUCCESS))
        result = NULL;
    if (result && (apr_pool_create(&self->pool, NULL) != APR_SUCCESS))
        result = NULL;

    if ((self->oom_buffer = malloc(64 * 1024))) 
        memset(self->oom_buffer, 0, 64 * 1024); /* make sure it's paged */
    else
        result = NULL;

    self->string_table = sie_string_table_new(self);
    self->string_literals = sie_string_table_init_literals(self->string_table);

    return result;
}

void sie_context_destroy(sie_Context *self)
{
    sie_vec_free(self->cleanup_fns);
    sie_vec_free(self->cleanup_targets);
    if (self->pool)
        apr_pool_destroy(self->pool);
    if (self->apr_initialize_result == APR_SUCCESS)
        apr_terminate();
    free(self->oom_buffer);
    if (self->num_destroys_p)
        (*self->num_destroys_p)++;
    adjust_alloc_record(self, self, -1);
    SIE_CONTEXT_OBJECT(self)->context = NULL;
    sie_context_object_destroy(SIE_CONTEXT_OBJECT(self));
}

SIE_OBJECT_NEW_FN(sie_context_new, sie_Context, self,
                  (void), sie_context_init(self));

SIE_CLASS(sie_Context, sie_Context_Object,
          SIE_MDEF(sie_copy, sie_copy_not_applicable)
          SIE_MDEF(sie_destroy, sie_context_destroy));

void sie_set_unhandled_exception_callback(void *ctx_obj,
                                          sie_Exception_Callback *cb,
                                          void *data)
{
    sie_Context *ctx = sie_context(ctx_obj);
    ctx->unhandled_exception_callback = cb;
    ctx->unhandled_exception_data = data;
}

void sie_set_api_exception_callback(void *ctx_obj,
                                    sie_Exception_Callback *cb,
                                    void *data)
{
    sie_Context *ctx = sie_context(ctx_obj);
    ctx->api_exception_callback = cb;
    ctx->api_exception_data = data;
}

sie_Exception *sie_check_exception(void *ctx_obj)
{
    if (ctx_obj) {
        sie_Context *self = sie_context(ctx_obj);
        sie_Exception *exception = self->exception;
        return exception;
    } else {
        return NULL;
    }
}

sie_Exception *sie_get_exception(void *ctx_obj)
{
    if (ctx_obj) {
        sie_Context *self = sie_context(ctx_obj);
        sie_Exception *exception = self->exception;
        self->exception = NULL;
        return exception;
    } else {
        return NULL;
    }
}

int sie_context_done(sie_Context *self)
{
    int remaining, will_be_destroyed;
    if (!self)
        return 0;
    sie_release(self->string_table);
    sie_release(sie_get_exception(self));
    will_be_destroyed = (sie_refcount(self) == 1);
    remaining = self->num_inits - (self->num_destroys +
                                   (will_be_destroyed ? 1 : 0));
#ifdef SIE_LEAK_CHECK
    if (remaining) {
        struct sie_alloc_record *cur;
        fprintf(stderr, "%d objects leaked:\n", remaining);
        for (cur = self->allocations; cur; cur = cur->hh.next) {
            fprintf(stderr, "p (*(%s *)%p)\n",
                    sie_object_class_name(cur->object),
                    cur->object);
        }
        abort();
    }
#endif
    sie_release(self);
    return remaining;
}

#undef SIE_VEC_CONTEXT_OBJECT
#define SIE_VEC_CONTEXT_OBJECT ctx_obj

size_t sie_cleanup_mark(void *ctx_obj)
{
    sie_Context *ctx = sie_context(ctx_obj);
    return sie_vec_size(ctx->cleanup_fns);
}

void *sie_cleanup_push(void *ctx_obj, sie_Cleanup_Fn *fn, void *target)
{
    sie_Context *ctx = sie_context(ctx_obj);
    sie_vec_push_back(ctx->cleanup_fns, fn);
    sie_vec_push_back(ctx->cleanup_targets, target);
    return target;
}

void sie_cleanup_pop(void *ctx_obj, void *target, int fire)
{
    sie_Context *ctx = sie_context(ctx_obj);
    sie_assertf(sie_vec_size(ctx->cleanup_fns) > 0,
                (ctx, "Tried to pop %p off an empty cleanup stack.", target));
    sie_assertf(*sie_vec_back(ctx->cleanup_targets) == target,
                (ctx, "Tried to pop %p, but %p was top of cleanup stack.",
                 target, *sie_vec_back(ctx->cleanup_targets)));
    sie_cleanup_pop_unchecked(ctx_obj, fire);
}

void sie_cleanup_pop_unchecked(void *ctx_obj, int fire)
{
    sie_Context *ctx = sie_context(ctx_obj);
    sie_Cleanup_Fn *fn = *sie_vec_back(ctx->cleanup_fns);
    void *target = *sie_vec_back(ctx->cleanup_targets);
    sie_vec_pop_back(ctx->cleanup_fns);
    sie_vec_pop_back(ctx->cleanup_targets);
    if (fire)
        fn(target);
}

void sie_cleanup_pop_mark(void *ctx_obj, size_t mark)
{
    sie_Context *ctx = sie_context(ctx_obj);
    while (sie_vec_size(ctx->cleanup_fns) > mark)
        sie_cleanup_pop_unchecked(ctx, 1);
}

void *sie_autorelease(void *object)
{
    if (object)
        return sie_cleanup_push(object, sie_release, object);
    else
        return NULL;
}

static void recursion_limit_pop(void *v_ctx)
{
    sie_Context *ctx = v_ctx;
    --ctx->recursion_count;
}

void sie_recursion_limit(void *ctx_obj)
{
    sie_Context *ctx = sie_context(ctx_obj);
    sie_cleanup_push(ctx, recursion_limit_pop, ctx);
    sie_assertf(++ctx->recursion_count < 100,
                (ctx, "Recursion limit reached."));
}

SIE_CONTEXT_OBJECT_NEW_FN(sie_error_context_new, sie_Error_Context,
                          self, ctx_obj,
                          (void *ctx_obj, sie_Error_Context *next,
                           const char *format, va_list args),
                          sie_error_context_init(self, ctx_obj, next,
                                                 format, args));

void sie_error_context_init(sie_Error_Context *self, void *ctx_obj,
                            sie_Error_Context *next,
                            const char *format, va_list args)
{
    sie_context_object_init(SIE_CONTEXT_OBJECT(self), ctx_obj);
    self->next = sie_retain(next);
    sie_vec_vprintf(ctx_obj, &self->message, format, args);
}

void sie_error_context_destroy(sie_Error_Context *self)
{
    sie_vec_free(self->message);
    sie_release(self->next);
    sie_context_object_destroy(SIE_CONTEXT_OBJECT(self));
}

void sie_error_context_vpush(void *ctx_obj, const char *format, va_list args)
{
    sie_Context *ctx = sie_context(ctx_obj);
    sie_Error_Context *ec =
        sie_error_context_new(ctx, ctx->error_context_top, format, args);
    ctx->error_context_top = ec;
}

void sie_error_context_push(void *ctx_obj, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    SIE_UNWIND_PROTECT(ctx_obj) {
        sie_error_context_vpush(ctx_obj, format, args);
    } SIE_CLEANUP() {
        va_end(args);
    } SIE_END_CLEANUP();
}

void sie_error_context_auto(void *ctx_obj, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    SIE_UNWIND_PROTECT(ctx_obj) {
        sie_error_context_vpush(ctx_obj, format, args);
        sie_cleanup_push(ctx_obj, sie_error_context_pop, ctx_obj);
    } SIE_CLEANUP() {
        va_end(args);
    } SIE_END_CLEANUP();
}

void sie_error_context_pop(void *ctx_obj)
{
    sie_Context *ctx = sie_context(ctx_obj);
    sie_Error_Context *ec = ctx->error_context_top;
    sie_assertf(ec, (ctx, "Tried to pop an empty error context stack."));
    ctx->error_context_top = ec->next;
    sie_release(ec);
}

SIE_CLASS(sie_Error_Context, sie_Context_Object,
          SIE_MDEF(sie_destroy, sie_error_context_destroy));

void *sie_malloc(void *ctx_obj, size_t size)
{
    void *ptr = malloc(size);
    if (!ptr)
        sie_throw_oom(ctx_obj);
    return ptr;
}

void *sie_calloc(void *ctx_obj, size_t size)
{
    void *ptr = calloc(1, size);
    if (!ptr)
        sie_throw_oom(ctx_obj);
    return ptr;
}

void *sie_realloc(void *ctx_obj, void *ptr, size_t size)
{
    void *newptr = realloc(ptr, size);
    if (!newptr)
        sie_throw_oom(ctx_obj);
    return newptr;
}

void *sie_memdup(void *ctx_obj, const void *data, size_t size)
{
    void *ptr = sie_malloc(ctx_obj, size);
    memcpy(ptr, data, size);
    return ptr;
}

char *sie_strdup(void *ctx_obj, const char *string)
{
    char *ptr = strdup(string);
    if (!ptr)
        sie_throw_oom(ctx_obj);
    return ptr;
}

void *sie_ctx_alloc(sie_Class *class_, void *ctx_obj)
{
    void *self = sie_calloc(ctx_obj, class_->size);
    sie_alloc_in_place(class_, self);
    return self;
}

void sie_ignore_trailing_garbage(void *ctx_obj, size_t amount)
{
    if (ctx_obj) {
        sie_Context *ctx = sie_context(ctx_obj);
        ctx->ignore_trailing_garbage = amount;
    }
}

void sie_set_progress_callbacks(void *ctx_obj, void *data, 
                                sie_Progress_Set_Message *set_message_callback, 
                                sie_Progress_Percent *percent_callback)
{
    if (ctx_obj) {
        sie_Context *ctx = sie_context(ctx_obj);
        ctx->progress_percent_last = -1;
        ctx->progress_data = data;
        ctx->progress_set_message = set_message_callback;
        ctx->progress_percent = percent_callback;
        ctx->progress_enabled =
            (set_message_callback || percent_callback) ? 1 : 0;
    }
}

void _sie_progress_msg(sie_Context *ctx, const char *msg)
{
    ctx->progress_percent_last = -1;
    if (ctx->progress_set_message)
        if (ctx->progress_set_message(ctx->progress_data, msg))
            sie_throw(sie_operation_aborted_new(ctx));
}

void _sie_progress(sie_Context *ctx,
                   sie_uint64 done, sie_uint64 total)
{
    if (ctx->progress_percent) {
        sie_float64 pct;
        if (done > total)
            done = total;
        if (total == 0) 
            ++total;  /* KLUDGE - avoid divide by zero */
        pct = (sie_float64)done / (sie_float64)total * 100.0;
        if (ctx->progress_percent_last != (int)pct) {
            ctx->progress_percent_last = (int)pct;
            if (ctx->progress_percent(ctx->progress_data, pct))
                sie_throw(sie_operation_aborted_new(ctx));
        }
    }
}
