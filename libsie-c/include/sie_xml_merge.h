/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

typedef struct _sie_XML_Definition sie_XML_Definition;

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_XML_MERGE_H
#define SIE_XML_MERGE_H

struct _sie_XML_Definition {
    sie_Context_Object parent;
    sie_XML_Incremental_Parser *parser;
    sie_XML *sie_node;
    sie_Id_Map *channel_map;
    sie_Id_Map *test_map;
    sie_Id_Map *decoder_map;
    sie_Id_Map *compiled_decoder_map;
    int sie_node_started;
    int any_private_attrs;
    /* Cached literals */
    sie_String *ch;
    sie_String *base;
    sie_String *data;
    sie_String *decoder;
    sie_String *dim;
    sie_String *id;
    sie_String *index;
    sie_String *private_;
    sie_String *sie;
    sie_String *tag;
    sie_String *test;
    sie_String *units;
    sie_String *xform;
};
SIE_CLASS_DECL(sie_XML_Definition);
#define SIE_XML_DEFINITION(p) SIE_SAFE_CAST(p, sie_XML_Definition)

SIE_DECLARE(sie_XML_Definition *) sie_xml_definition_new(void *ctx_obj);

SIE_DECLARE(void) sie_xml_definition_init(sie_XML_Definition *self,
                                          void *ctx_obj);
SIE_DECLARE(void) sie_xml_definition_destroy(sie_XML_Definition *self);

SIE_DECLARE(void) sie_xml_definition_add_string(sie_XML_Definition *self,
                                                const char *string,
                                                size_t size);

SIE_DECLARE(sie_XML *) sie_xml_expand(sie_XML_Definition *xml,
                                      sie_String *type, sie_id id);

#endif

#endif
