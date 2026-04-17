/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#include "sie_config.h"

#include "sie_ref.h"

void sie_ref_init(sie_Ref *self, void *ctx_obj)
{
    sie_context_object_init(SIE_CONTEXT_OBJECT(self), ctx_obj);
}

void sie_ref_destroy(sie_Ref *self)
{
    sie_context_object_destroy(SIE_CONTEXT_OBJECT(self));
}

static void bad_spigot(sie_Ref *self) {
    sie_errorf((self, "Tried to attach a spigot to unspigotable object %p "
                "of class '%s'.", self, sie_object_class_name(self)));
}

static void bad_ref_access(sie_Ref *self) {
    sie_errorf((self, "Tried a bad sie_Ref access method on object %p "
                "of class '%s'.", self, sie_object_class_name(self)));
}

/*
SIE_VOID_API_METHOD(sie_setprop_uint32, self,
                    (void *self, char *prop, sie_uint32 value),
                    (self, prop, value));

SIE_VOID_API_METHOD(sie_setprop_float64, self,
                    (void *self, char *prop, sie_float64 value),
                    (self, prop, value));
*/

#define DEFMETH(name, type, error_value) \
    SIE_API_METHOD(name, type, error_value, self, (void *self), (self));

SIE_API_METHOD(sie_get_test, sie_Test *, NULL,
               self, (void *self, sie_id id), (self, id));
SIE_API_METHOD(sie_get_channel, sie_Channel *, NULL,
               self, (void *self, sie_id id), (self, id));
SIE_API_METHOD(sie_get_dimension, sie_Dimension *, NULL,
               self, (void *self, sie_id index), (self, index));

DEFMETH(sie_attach_spigot, sie_Spigot *, NULL);

DEFMETH(sie_get_tests, sie_Iterator *, NULL);
DEFMETH(sie_get_all_tests, sie_Iterator *, NULL);
DEFMETH(sie_get_channels, sie_Iterator *, NULL);
DEFMETH(sie_get_all_channels, sie_Iterator *, NULL);
DEFMETH(sie_get_dimensions, sie_Iterator *, NULL);
DEFMETH(sie_get_tags, sie_Iterator *, NULL);

DEFMETH(sie_get_containing_file, sie_File *, NULL);
DEFMETH(sie_get_containing_test, sie_Test *, NULL);

DEFMETH(sie_get_name_s, sie_String *, NULL);
DEFMETH(sie_get_name, const char *, NULL);
DEFMETH(sie_get_index, sie_id, SIE_NULL_ID);
DEFMETH(sie_get_base, sie_id, SIE_NULL_ID);
DEFMETH(sie_get_id, sie_id, SIE_NULL_ID);

SIE_API_METHOD(sie_get_tag, sie_Tag *, NULL, self,
               (void *self, const char *id), (self, id));
SIE_API_METHOD(sie_get_tag_value_b, int, 0, self,
               (void *self, const char *id, char **value, size_t *size),
               (self, id, value, size));
SIE_API_METHOD(sie_get_tag_value, char *, NULL, self,
               (void *self, const char *id), (self, id));

#undef DEFMETH

sie_Tag *sie_ref_get_tag(sie_Ref *self, const char *id)
{
    sie_String *id_s = sie_string_find(self, id, strlen(id));
    sie_Iterator *tags;
    sie_Tag *tag;

    if (!id_s) /* tag can't possibly exist if id not in stringtable */
        return NULL;

    tags = sie_autorelease(sie_get_tags(self));
    while ((tag = sie_iterator_next(tags))) {
        if (sie_tag_get_id_s(tag) == id_s) {
            sie_retain(tag);
            goto out;
        }
    }
    tag = NULL;
out:
    sie_cleanup_pop(self, tags, 1);
    return tag;
}

int sie_ref_get_tag_value_b(sie_Ref *self, const char *id,
                            char **value, size_t *size)
{
    sie_Tag *tag = sie_autorelease(sie_get_tag(self, id));
    int result = sie_tag_get_value_b(tag, value, size);
    if (tag)
        sie_cleanup_pop(self, tag, 1);
    return result;
}

char *sie_ref_get_tag_value(sie_Ref *self, const char *id)
{
    char *value = NULL;
    size_t size = 0;
    if (sie_get_tag_value_b(self, id, &value, &size))
        return value;
    else
        return NULL;
}

char *sie_ref_get_name(sie_Ref *self)
{
    return sie_string_value(sie_get_name_s(self));
}

SIE_CLASS(sie_Ref, sie_Context_Object,
          SIE_MDEF(sie_destroy, sie_ref_destroy)

          SIE_MDEF(sie_get_test, bad_ref_access)
          SIE_MDEF(sie_get_channel, bad_ref_access)
          SIE_MDEF(sie_get_dimension, bad_ref_access)

          SIE_MDEF(sie_attach_spigot, bad_spigot)

          SIE_MDEF(sie_get_tests, bad_ref_access)
          SIE_MDEF(sie_get_all_tests, bad_ref_access)
          SIE_MDEF(sie_get_channels, bad_ref_access)
          SIE_MDEF(sie_get_all_channels, bad_ref_access)
          SIE_MDEF(sie_get_dimensions, bad_ref_access)
          SIE_MDEF(sie_get_tags, bad_ref_access)

          SIE_MDEF(sie_get_containing_file, bad_ref_access)
          SIE_MDEF(sie_get_containing_test, bad_ref_access)

          SIE_MDEF(sie_get_tag, sie_ref_get_tag)
          SIE_MDEF(sie_get_tag_value_b, sie_ref_get_tag_value_b)
          SIE_MDEF(sie_get_tag_value, sie_ref_get_tag_value)

          SIE_MDEF(sie_get_name, sie_ref_get_name)
          SIE_MDEF(sie_get_index, bad_ref_access)
          SIE_MDEF(sie_get_base, bad_ref_access)
          SIE_MDEF(sie_get_id, bad_ref_access));

