/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

typedef struct _sie_String sie_String;
typedef struct _sie_String_Table sie_String_Table;

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_STRINGTABLE_H
#define SIE_STRINGTABLE_H

enum {
    SIE_LITERAL_NULL = 0,

#define SIE_LITERAL(x) SIE_LITERAL_##x,
#include "sie_literals.h"
#undef SIE_LITERAL

    SIE_NUM_STRING_LITERALS
};

#define sie_literal(ctx_obj, x) \
    (SIE_CONTEXT_OBJECT(ctx_obj)->context->string_literals[SIE_LITERAL_##x])

#include "sie_uthash.h"

struct _sie_String {
    sie_Context_Object parent;
    char *value;
    size_t size;
    UT_hash_handle hh;
};
SIE_CLASS_DECL(sie_String);
#define SIE_STRING(p) SIE_SAFE_CAST(p, sie_String)

struct _sie_String_Table {
    sie_Context_Object parent;
    sie_String **string_literals;
    sie_String *table_head;
};
SIE_CLASS_DECL(sie_String_Table);
#define SIE_STRING_TABLE(p) SIE_SAFE_CAST(p, sie_String_Table)

#define sie_sv(s) ((s)->value)
#define sie_slen(s) ((s)->size)

#define sie_string_value(s) ((s) ? sie_sv(s) : NULL)
#define sie_string_length(s) ((s) ? sie_slen(s) : 0)

SIE_DECLARE(sie_String *) sie_string_find(void *ctx_obj, const char *value,
                                          size_t size);
SIE_DECLARE(sie_String *) sie_string_get(void *ctx_obj, const char *value,
                                         size_t size);

SIE_DECLARE(sie_String_Table *) sie_string_table_new(void *ctx_obj);
SIE_DECLARE(sie_String **) sie_string_table_init_literals(
    sie_String_Table *self);

#endif

#endif
