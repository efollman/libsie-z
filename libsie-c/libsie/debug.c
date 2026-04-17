/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#include "sie_config.h"

#include "sie_debug.h"

#if SIE_DEBUG >= 0

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "sie_context.h"

void _sie_vdebug(void *ctx_obj, int lvl, char *fmt, va_list args)
{
    sie_Context *ctx = sie_context(ctx_obj);
    vfprintf(ctx->debug_stream, fmt, args);
}

void _sie_debug(void *ctx_obj, int lvl, char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    _sie_vdebug(ctx_obj, lvl, fmt, args);
    va_end(args);
}

#endif /* SIE_DEBUG */
