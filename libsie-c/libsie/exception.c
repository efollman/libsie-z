/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#include "sie_config.h"

#include "sie_exception.h"
#include "sie_vec.h"

void _sie_install_handler(sie_Context *ctx, sie_Handler *handler,
                          const char *file, int line)
{
    handler->next = ctx->top_handler;
    handler->context = ctx;
    handler->exception = NULL;
    handler->rethrow = 0;
    handler->cleanup_mark = sie_cleanup_mark(ctx);
    handler->error_context = ctx->error_context_top;
    handler->file = file;
    handler->line = line;
    ctx->top_handler = handler;
}

void _sie_remove_handler(sie_Handler *handler)
{
    /* FIXME make sanity checks a little less harsh */
    if (handler->context->top_handler != handler)
        abort();
#if 0
    if (sie_cleanup_mark(handler->context) != handler->cleanup_mark)
        abort();
    if (handler->context->error_context_top != handler->error_context)
        abort();
#endif

    handler->context->top_handler = handler->next;
}

void _sie_maybe_throw(void *v_self, char *file, int line)
{
    sie_Exception *self = SIE_EXCEPTION(v_self);
    sie_Context *context = sie_context(self);
    sie_Handler *handler = context->top_handler;

    if (!self->file) {          /* don't reset if rethrown! */
        self->file = file;
        self->line = line;
    }
    if (!self->error_context) {
        self->error_context = sie_retain(context->error_context_top);
    }

    if (handler == NULL)
        return;
    
    context->top_handler = handler->next;
    handler->exception = self;
    sie_cleanup_pop_mark(context, handler->cleanup_mark);
    while (context->error_context_top != handler->error_context)
        sie_error_context_pop(context);
    longjmp(handler->jump_buffer, 1);
}

void _sie_throw(void *v_self, char *file, int line)
{
    _sie_maybe_throw(v_self, file, line);
    {
        sie_Context *ctx = sie_context(v_self);
        if (ctx->unhandled_exception_callback) {
            ctx->unhandled_exception_callback(v_self,
                                              ctx->unhandled_exception_data);
        } else {
            fprintf(stderr, "\n*** Uncaught exception (%s):\n  %s\n",
                    sie_object_class_name(v_self), sie_verbose_report(v_self));
        }
    }
    abort();
}

void sie_throw_oom(void *ctx_obj)
{
    sie_Context *ctx = sie_context(ctx_obj);
    if (!ctx->oom_buffer) {
        /* We're screwed - we've already consumed our OOM buffer, so
         * there's no way we can continue. */
        abort();
    }
    free(ctx->oom_buffer);
    ctx->oom_buffer = NULL;
    sie_throw(sie_out_of_memory_new(ctx_obj));
}

void sie_exception_init(sie_Exception *self, void *ctx_obj)
{
    sie_context_object_init(SIE_CONTEXT_OBJECT(self), ctx_obj);
}

void *sie_exception_copy(sie_Exception *self)
{
    sie_Exception *copy = sie_context_object_copy(SIE_CONTEXT_OBJECT(self));
    copy->report_string = NULL;
    if (self->report_string)
        sie_vec_printf(self, &copy->report_string, "%s", self->report_string);
    return copy;
}

SIE_VOID_METHOD(sie_generate_report_string, self, (void *self), (self));

void sie_exception_generate_report_string(sie_Exception *self)
{
    sie_vec_printf(self, &self->report_string, "An exception occurred.");
}

SIE_API_METHOD(sie_report, char *, NULL, self, (void *self), (self));

char *sie_exception_report(sie_Exception *self)
{
    sie_generate_report_string(self);
    sie_vec_strcatf(self, &self->report_string, " (at %s:%d)",
                    self->file, self->line);
    return self->report_string;
}

SIE_API_METHOD(sie_verbose_report, char *, NULL, self, (void *self), (self));

char *sie_exception_verbose_report(sie_Exception *self)
{
    sie_Error_Context *cur = self->error_context;
    sie_report(self);
    if (cur)
        sie_vec_strcatf(self, &self->report_string, "\n    while:");
    for ( ; cur != NULL; cur = cur->next) {
        sie_vec_strcatf(self, &self->report_string, "\n    %s", cur->message);
    }
    return self->report_string;
}


void sie_exception_destroy(sie_Exception *self)
{
    sie_release(self->error_context);
    sie_vec_free(self->report_string);
    sie_context_object_destroy(SIE_CONTEXT_OBJECT(self));
}

void sie_exception_save(sie_Exception *self)
{
    sie_Context *ctx = sie_context(self);
    if (ctx->exception != self) {
        sie_release(ctx->exception);
        sie_retain(self);
        ctx->exception = self;
    }
    if (ctx->api_exception_callback)
        ctx->api_exception_callback(self, ctx->api_exception_data);
}


SIE_CONTEXT_OBJECT_NEW_FN(sie_exception_new, sie_Exception, self, ctx_obj,
                          (void *ctx_obj), sie_exception_init(self, ctx_obj));

SIE_CLASS(sie_Exception, sie_Context_Object,
          SIE_MDEF(sie_generate_report_string,
                   sie_exception_generate_report_string)
          SIE_MDEF(sie_report, sie_exception_report)
          SIE_MDEF(sie_verbose_report, sie_exception_verbose_report)
          SIE_MDEF(sie_copy, sie_exception_copy)
          SIE_MDEF(sie_destroy, sie_exception_destroy));

void sie_simple_error_init(sie_Simple_Error *self, void *ctx_obj,
                           const char *format, va_list args)
{
    sie_exception_init(SIE_EXCEPTION(self), ctx_obj);
    sie_vec_vprintf(self, &self->simple_error_string, format, args);
}

SIE_CONTEXT_OBJECT_VARARGS_NEW_FN(
    sie_simple_error_new, sie_Simple_Error, self, ctx_obj, args, format,
    (void *ctx_obj, const char *format, ...),
    sie_simple_error_init(self, ctx_obj, format, args));


void sie_simple_error_generate_report_string(sie_Simple_Error *self)
{
    sie_vec_printf(self, &SIE_EXCEPTION(self)->report_string, "%s",
                   self->simple_error_string);
}

void sie_simple_error_destroy(sie_Simple_Error *self)
{
    sie_vec_free(self->simple_error_string);
    sie_exception_destroy(SIE_EXCEPTION(self));
}

SIE_CLASS(sie_Simple_Error, sie_Exception,
          SIE_MDEF(sie_generate_report_string,
                   sie_simple_error_generate_report_string)
          SIE_MDEF(sie_destroy, sie_simple_error_destroy));

SIE_CONTEXT_OBJECT_NEW_FN(sie_operation_aborted_new, sie_Operation_Aborted,
                          self, ctx_obj, (void *ctx_obj),
                          sie_exception_init(SIE_EXCEPTION(self), ctx_obj));

void sie_operation_aborted_generate_report_string(sie_Operation_Aborted *self)
{
    sie_vec_printf(self, &SIE_EXCEPTION(self)->report_string,
                   "Operation aborted.");
}

SIE_CLASS(sie_Operation_Aborted, sie_Exception,
          SIE_MDEF(sie_generate_report_string,
                   sie_operation_aborted_generate_report_string));

SIE_CONTEXT_OBJECT_NEW_FN(sie_out_of_memory_new, sie_Out_Of_Memory,
                          self, ctx_obj, (void *ctx_obj),
                          sie_exception_init(SIE_EXCEPTION(self), ctx_obj));

void sie_out_of_memory_generate_report_string(sie_Out_Of_Memory *self)
{
    sie_vec_printf(self, &SIE_EXCEPTION(self)->report_string,
                   "Out of memory.");
}

SIE_CLASS(sie_Out_Of_Memory, sie_Exception,
          SIE_MDEF(sie_generate_report_string,
                   sie_out_of_memory_generate_report_string));
