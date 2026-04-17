/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

typedef struct _sie_Tag sie_Tag;
typedef struct _sie_Tag_Spigot sie_Tag_Spigot;

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_TAG_H
#define SIE_TAG_H

struct _sie_Tag {
    sie_Ref parent;
    sie_XML *node;
    sie_Intake *intake;
    sie_String *id;
    sie_String *value;
    sie_id group;
};
SIE_CLASS_DECL(sie_Tag);
#define SIE_TAG(p) SIE_SAFE_CAST(p, sie_Tag)

SIE_DECLARE(sie_Tag *) sie_tag_new(void *intake, sie_XML *node);
SIE_DECLARE(void) sie_tag_init(sie_Tag *self, void *intake, sie_XML *node);
SIE_DECLARE(void) sie_tag_destroy(sie_Tag *self);
SIE_DECLARE(void) sie_tag_dump(sie_Tag *self, FILE *stream, char *prefix);

SIE_DECLARE(sie_Spigot *) sie_tag_attach_spigot(sie_Tag *self);

SIE_DECLARE(const char *) sie_tag_get_id(sie_Tag *self);
SIE_DECLARE(int) sie_tag_get_value_b(sie_Tag *self, 
                                     char **value, size_t *size);
SIE_DECLARE(char *) sie_tag_get_value(sie_Tag *self);
SIE_DECLARE(sie_id) sie_tag_get_group(sie_Tag *self);
SIE_DECLARE(int) sie_tag_is_from_group(sie_Tag *self);

SIE_DECLARE(sie_uint64) sie_tag_get_value_size(sie_Tag *self);
SIE_DECLARE(sie_String *) sie_tag_get_id_s(sie_Tag *self);
SIE_DECLARE(sie_String *) sie_tag_get_value_s(sie_Tag *self);

SIE_DECLARE(void *) sie_tag_xml_iterator_realize(void *ctx_obj, void *v_xml,
                                                 void *v_intake);

struct _sie_Tag_Spigot {
    sie_Spigot parent;
    sie_Tag *tag;
    sie_Output *output;
    int has_been_output;
};
SIE_CLASS_DECL(sie_Tag_Spigot);
#define SIE_TAG_SPIGOT(p) SIE_SAFE_CAST(p, sie_Tag_Spigot);

SIE_DECLARE(sie_Tag_Spigot *) sie_tag_spigot_new(sie_Tag *tag);
SIE_DECLARE(void) sie_tag_spigot_init(sie_Tag_Spigot *self, sie_Tag *tag);
SIE_DECLARE(void) sie_tag_spigot_destroy(sie_Tag_Spigot *self);

SIE_DECLARE(sie_Output *) sie_tag_spigot_get_inner(sie_Tag_Spigot *self);

SIE_DECLARE(void) sie_tag_spigot_clear_output(sie_Tag_Spigot *self);

#endif

#endif
