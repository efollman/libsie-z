/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#define HASH_FUNCTION HASH_BER

#include "sie_config.h"

static void sie_string_init(sie_String *self, void *ctx_obj,
                            const char *value, size_t size)
{
    sie_context_object_init(SIE_CONTEXT_OBJECT(self), ctx_obj);
    self->value = sie_malloc(self, size + 1);
    memcpy(self->value, value, size);
    self->size = size;
    self->value[size] = 0;
    sie_debug((self, 20, "sie_String +++ '%s'\n", self->value));
}

static void sie_string_destroy(sie_String *self)
{
    sie_String_Table *st = sie_string_table(self);
    HASH_DELETE(hh, st->table_head, self);
    sie_debug((self, 20, "sie_String --- '%s'\n", self->value));
    free(self->value);
    sie_context_object_destroy(SIE_CONTEXT_OBJECT(self));
}

static SIE_CONTEXT_OBJECT_NEW_FN(
    sie_string_new, sie_String, self, ctx_obj,
    (void *ctx_obj, const char *value, size_t size),
    sie_string_init(self, ctx_obj, value, size));

SIE_CLASS(sie_String, sie_Context_Object,
          SIE_MDEF(sie_destroy, sie_string_destroy)
          SIE_MDEF(sie_copy, sie_copy_not_applicable));


const static char *literals[] = {
    "",
#define SIE_LITERAL(x) #x,
#include "sie_literals.h"
#undef SIE_LITERAL
};

sie_String **sie_string_table_init_literals(sie_String_Table *self)
{
    size_t i;
    self->string_literals = sie_malloc(self, sizeof(self->string_literals) *
                                       SIE_NUM_STRING_LITERALS);
    for (i = 0; i < SIE_NUM_STRING_LITERALS; i++) {
        self->string_literals[i] =
            sie_string_get(self, literals[i], strlen(literals[i]));
    }
    return self->string_literals;
}

static void sie_string_table_init(sie_String_Table *self, void *ctx_obj)
{
    sie_context_object_init(SIE_CONTEXT_OBJECT(self), ctx_obj);
}

static void sie_string_table_destroy(sie_String_Table *self)
{
    size_t i;
    for (i = 0; i < SIE_NUM_STRING_LITERALS; i++)
        sie_release(self->string_literals[i]);
    free(self->string_literals);
    sie_context_object_destroy(SIE_CONTEXT_OBJECT(self));
}

SIE_CONTEXT_OBJECT_NEW_FN(
    sie_string_table_new, sie_String_Table, self, ctx_obj,
    (void *ctx_obj), sie_string_table_init(self, ctx_obj));

static sie_String *find(sie_String_Table *self, const char *value, size_t size)
{
    sie_String *found;
    HASH_FIND(hh, self->table_head, value, (int)size, found);
    return found;
}

sie_String *sie_string_find(void *ctx_obj, const char *value, size_t size)
{
    if (value)
        return find(sie_string_table(ctx_obj), value, size);
    else
        return NULL;
}

sie_String *sie_string_get(void *ctx_obj, const char *value, size_t size)
{
    if (value) {
        sie_String_Table *self = sie_string_table(ctx_obj);
        sie_String *found = find(self, value, size);
        
        if (found) {
            return sie_retain(found);
        } else {
            found = sie_string_new(self, value, size);
            HASH_ADD_KEYPTR(hh, self->table_head, found->value,
                            (int)size, found);
            return found;
        }
    }
    return NULL;
}

SIE_CLASS(sie_String_Table, sie_Context_Object,
          SIE_MDEF(sie_destroy, sie_string_table_destroy)
          SIE_MDEF(sie_copy, sie_copy_not_applicable));
