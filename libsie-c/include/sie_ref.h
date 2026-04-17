/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

typedef struct _sie_Ref sie_Ref;

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_REF_H
#define SIE_REF_H

struct _sie_Ref {
    sie_Context_Object parent;
};
SIE_CLASS_DECL(sie_Ref);
#define SIE_REF(p) SIE_SAFE_CAST(p, sie_Ref)

SIE_DECLARE(void) sie_ref_init(sie_Ref *self, void *ctx_obj);
SIE_DECLARE(void) sie_ref_destroy(sie_Ref *self);

/*
SIE_METHOD_DECL(sie_setprop_uint32);
SIE_DECLARE(void) sie_setprop_uint32(void *self, char *prop, sie_uint32 value);

SIE_METHOD_DECL(sie_setprop_float64);
SIE_DECLARE(void) sie_setprop_float64(void *self, char *prop,
                                      sie_float64 value);
*/

#define DEFMETH(type, name)                     \
    SIE_METHOD_DECL(name);                      \
    SIE_DECLARE(type) name(void *self);

SIE_METHOD_DECL(sie_get_test);
SIE_DECLARE(sie_Test *) sie_get_test(void *self, sie_id id);
SIE_METHOD_DECL(sie_get_channel);
SIE_DECLARE(sie_Channel *) sie_get_channel(void *self, sie_id id);
SIE_METHOD_DECL(sie_get_dimension);
SIE_DECLARE(sie_Dimension *) sie_get_dimension(void *self, sie_id index);

DEFMETH(sie_Spigot *, sie_attach_spigot);

DEFMETH(sie_Iterator *, sie_get_tests);
DEFMETH(sie_Iterator *, sie_get_all_tests);
DEFMETH(sie_Iterator *, sie_get_channels);
DEFMETH(sie_Iterator *, sie_get_all_channels);
DEFMETH(sie_Iterator *, sie_get_dimensions);
DEFMETH(sie_Iterator *, sie_get_tags);

DEFMETH(sie_File *, sie_get_containing_file);
DEFMETH(sie_Test *, sie_get_containing_test);

DEFMETH(sie_String *, sie_get_name_s);
DEFMETH(const char *, sie_get_name);
DEFMETH(sie_id, sie_get_index);
DEFMETH(sie_id, sie_get_base);
DEFMETH(sie_id, sie_get_id);

SIE_METHOD_DECL(sie_get_tag);
SIE_DECLARE(sie_Tag *) sie_get_tag(void *self, const char *id);
SIE_METHOD_DECL(sie_get_tag_value_b);
SIE_DECLARE(int) sie_get_tag_value_b(void *self, const char *id,
                                     char **value, size_t *size);
SIE_METHOD_DECL(sie_get_tag_value);
SIE_DECLARE(char *) sie_get_tag_value(void *self, const char *id);

#undef DEFMETH

/*
 * get_channel_by_id
 */

#endif

#endif
