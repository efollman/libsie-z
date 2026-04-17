/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_DEBUG_H
#define SIE_DEBUG_H

#ifndef SIE_DEBUG
#define SIE_DEBUG -1
#endif

#if SIE_DEBUG >= 0

#include <stdarg.h>

#define sie_debugging(ctx, lvl)                 \
    ((SIE_DEBUG) >= (lvl) &&                    \
     sie_context(ctx)->debug_stream &&          \
     sie_context(ctx)->debug_level >= (lvl))

#define sie_when_debugging(ctx, lvl, call)      \
    do {                                        \
        if (sie_debugging(ctx, lvl)) {          \
            call;                               \
        }                                       \
    } while (0)

#define sie_vdebug(ctx, lvl, fmt, args)                                 \
    sie_when_debugging(ctx, lvl, _sie_vdebug(ctx, lvl, fmt, args))

#ifdef SIE_HAS_C99_MACROS
#define sie_debug(args) SIE_DEBUG_C99 args
#define SIE_DEBUG_C99(ctx, lvl, ...)                                    \
    sie_when_debugging(ctx, lvl, _sie_debug(ctx, lvl, __VA_ARGS__))
#else
#define sie_debug(args)
#endif

SIE_DECLARE(void) _sie_vdebug(void *ctx_obj, int lvl, char *fmt, va_list args);
SIE_DECLARE_NONSTD(void) _sie_debug(void *ctx_obj, int lvl, char *fmt, ...)
    __gcc_attribute__ ((format(printf, 3, 4)));

#else /* SIE_DEBUG < 0 */

#define sie_vdebug(ctx, lvl, fmt, args)
#define sie_debug(args)

#endif /* SIE_DEBUG */

#endif

#endif
