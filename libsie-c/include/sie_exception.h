/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

typedef struct _sie_Exception sie_Exception;
typedef struct _sie_Handler sie_Handler;

typedef struct _sie_Simple_Error sie_Simple_Error;
typedef struct _sie_Operation_Aborted sie_Operation_Aborted;
typedef struct _sie_Out_Of_Memory sie_Out_Of_Memory;

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_EXCEPTION_H
#define SIE_EXCEPTION_H

#include <stdarg.h>
#include <setjmp.h>

struct _sie_Handler {
    sie_Context *context;
    jmp_buf jump_buffer;
    sie_Exception *exception;
    int rethrow;
    size_t cleanup_mark;
    sie_Error_Context *error_context;
    sie_Handler *next;
    const char *file;
    int line;
};

SIE_DECLARE(void) _sie_install_handler(sie_Context *ctx, sie_Handler *handler,
                                       const char *file, int line);
SIE_DECLARE(void) _sie_remove_handler(sie_Handler *handler);

struct _sie_Exception {
    sie_Context_Object parent;
    char *report_string;
    char *file;
    int line;
    sie_Error_Context *error_context;
};
SIE_CLASS_DECL(sie_Exception);
#define SIE_EXCEPTION(p) SIE_SAFE_CAST(p, sie_Exception)

SIE_DECLARE(void) _sie_maybe_throw(void *v_self, char *file, int line);
#define sie_maybe_throw(self) _sie_maybe_throw(self, __FILE__, __LINE__)
SIE_DECLARE(void) _sie_throw(void *v_self, char *file, int line);
#define sie_throw(self) _sie_throw(self, __FILE__, __LINE__)

SIE_DECLARE(void) sie_throw_oom(void *ctx_obj);

SIE_METHOD_DECL(sie_generate_report_string);
SIE_DECLARE(void) sie_generate_report_string(void *self);
SIE_DECLARE(void) sie_exception_generate_report_string(sie_Exception *self);

SIE_METHOD_DECL(sie_report);
SIE_DECLARE(char *) sie_report(void *self);
SIE_METHOD_DECL(sie_verbose_report);
SIE_DECLARE(char *) sie_verbose_report(void *self);

SIE_DECLARE(char *) sie_exception_report(sie_Exception *self);
SIE_DECLARE(char *) sie_exception_verbose_report(sie_Exception *self);

SIE_DECLARE(void) sie_exception_destroy(sie_Exception *self);

SIE_DECLARE(sie_Exception *) sie_exception_new(void *ctx_obj);
SIE_DECLARE(void) sie_exception_init(sie_Exception *self, void *ctx_obj);

SIE_DECLARE(void) sie_exception_save(sie_Exception *self);

struct _sie_Simple_Error {
    sie_Exception parent;
    char *simple_error_string;
};
SIE_CLASS_DECL(sie_Simple_Error);
#define SIE_SIMPLE_ERROR(p) SIE_SAFE_CAST(p, sie_Simple_Error)

SIE_DECLARE(void) sie_simple_error_init(sie_Simple_Error *self, void *ctx_obj,
                                        const char *format, va_list args);
SIE_DECLARE_NONSTD(sie_Simple_Error *) sie_simple_error_new(
    void *ctx_obj, const char *format, ...)
    __gcc_attribute__ ((format(printf, 2, 3)));
SIE_DECLARE(void) sie_simple_error_generate_report_string(
    sie_Simple_Error *self);
SIE_DECLARE(void) sie_simple_error_destroy(sie_Simple_Error *self);

#define sie_assert(test, ctx_obj)                               \
    do {                                                        \
        if (!(test))                                            \
            sie_throw(sie_simple_error_new(                     \
                          ctx_obj, "Assertion '%s' failed",     \
                          #test));                              \
    } while (0)

#define sie_errorf(args)                        \
    do {                                        \
        sie_throw(sie_simple_error_new args);   \
    } while (0)

#define sie_assertf(test, simple_error_args)    \
    do {                                        \
        if (!(test))                            \
            sie_errorf(simple_error_args);      \
    } while (0)

struct _sie_Operation_Aborted {
    sie_Exception parent;
};
SIE_CLASS_DECL(sie_Operation_Aborted);
#define SIE_OPERATION_ABORTED(p) SIE_SAFE_CAST(p, sie_Operation_Aborted)

SIE_DECLARE(sie_Operation_Aborted *) sie_operation_aborted_new(void *ctx_obj);
SIE_DECLARE(void) sie_operation_aborted_generate_report_string(
    sie_Operation_Aborted *self);

struct _sie_Out_Of_Memory {
    sie_Exception parent;
};
SIE_CLASS_DECL(sie_Out_Of_Memory);
#define SIE_OUT_OF_MEMORY(p) SIE_SAFE_CAST(p, sie_Out_Of_Memory)

SIE_DECLARE(sie_Out_Of_Memory *) sie_out_of_memory_new(void *ctx_obj);
SIE_DECLARE(void) sie_out_of_memory_generate_report_string(
    sie_Out_Of_Memory *self);

#define SIE_TRY(self)                                           \
    do {                                                        \
        sie_Handler _handler;                                   \
        _sie_install_handler(sie_context(self), &_handler,      \
                             __FILE__, __LINE__);               \
        do {                                                    \
            if (setjmp(_handler.jump_buffer) == 0) {            \
                do {

#define SIE_CATCH(ex)                           \
                } while (0);                    \
                _sie_remove_handler(&_handler); \
                break;                          \
            } else {                            \
                sie_Exception *ex;              \
                ex = _handler.exception;        \
                (void)ex;                       \
                do {

#define SIE_RETHROW() (_handler.rethrow = 1)

#define SIE_FINALLY()                                   \
                } while (0);                            \
                if (!_handler.rethrow)                  \
                    sie_release(_handler.exception);    \
            }                                           \
        } while (0);                                    \
        do {

#define SIE_END_FINALLY()                                       \
        } while (0);                                            \
        if (_handler.rethrow) sie_throw(_handler.exception);    \
    } while (0)

#define SIE_NO_FINALLY() \
    SIE_FINALLY() SIE_END_FINALLY()

#define SIE_UNWIND_PROTECT(self)                                \
    do {                                                        \
        sie_Handler _handler;                                   \
        _sie_install_handler(sie_context(self), &_handler,      \
                             __FILE__, __LINE__);               \
        if (setjmp(_handler.jump_buffer) == 0) {                \
            do {

#define SIE_CLEANUP()                           \
            } while (0);                        \
            _sie_remove_handler(&_handler);     \
        } else {                                \
            SIE_RETHROW();                      \
        }                                       \
        do {

#define SIE_END_CLEANUP()                                       \
        } while (0);                                            \
        if (_handler.rethrow) sie_throw(_handler.exception);    \
    } while (0)

#define SIE_API_TRY(self)                       \
    SIE_TRY(self)

#define SIE_END_API_TRY(caught)                 \
    SIE_CATCH(_e) {                             \
        sie_maybe_throw(_e);                    \
        sie_exception_save(_e);                 \
        caught = 1;                             \
        (void)caught;                           \
    } SIE_NO_FINALLY()

#define SIE_API_WRAPPER(self, fail)             \
    int volatile _error = 0;                    \
    if (!self) return fail;                     \
    SIE_API_TRY(self) {

#define SIE_END_API_WRAPPER(success, fail)      \
    } SIE_END_API_TRY(_error);                  \
    if (_error)                                 \
        return fail;                            \
    else                                        \
        return success;

#endif

#endif
