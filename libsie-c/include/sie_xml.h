/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

typedef struct _sie_XML_Attr sie_XML_Attr;
typedef struct _sie_XML_Element sie_XML_Element;
typedef struct _sie_XML_Text sie_XML_Text;
typedef struct _sie_XML sie_XML;
typedef struct _sie_XML_Incremental_Parser sie_XML_Incremental_Parser;

typedef struct _sie_XML_Parser sie_XML_Parser;

typedef struct _sie_XML_Pack_Head sie_XML_Pack_Head;
typedef struct _sie_XML_Pack_Tail sie_XML_Pack_Tail;

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_XML_H
#define SIE_XML_H

#include <stdlib.h>

struct _sie_XML_Attr {
    sie_String *name;
    sie_String *value;
};

struct _sie_XML_Element {
    sie_String *name;
    size_t num_attrs;
    sie_XML_Attr *attrs;
    sie_XML_Attr static_attrs[2];
};

struct _sie_XML_Text {
    sie_String *text;
};

enum sie_XML_Type {
    SIE_XML_ELEMENT = 1,
    SIE_XML_TEXT = 2,
    SIE_XML_COMMENT = 3,
    SIE_XML_PROCESSING_INSTRUCTION = 4,
};

enum sie_XML_Link_Type {
    SIE_XML_LINK_AFTER = 0,
    SIE_XML_LINK_BEFORE = 1
};

struct _sie_XML {
    sie_Context_Object _parent;
    enum sie_XML_Type type;
    union {
        sie_XML_Element element;
        sie_XML_Text text;
    } value;
    sie_XML *parent;
    sie_XML *next;
    sie_XML *prev;
    sie_XML *child;
    sie_XML *last_child;
};
SIE_CLASS_DECL(sie_XML);
#define SIE_XML(p) SIE_SAFE_CAST(p, sie_XML)

SIE_DECLARE(sie_XML *) sie_xml_new(void *ctx_obj);
SIE_DECLARE(void) sie_xml_init(sie_XML *self, void *ctx_obj);
SIE_DECLARE(void *) sie_xml_copy(sie_XML *self);
SIE_DECLARE(void) sie_xml_destroy(sie_XML *self);
SIE_DECLARE(void) sie_xml_set_attribute_s(sie_XML *self,
                                          sie_String *name, sie_String *value);
SIE_DECLARE(void) sie_xml_set_attribute_b(sie_XML *self,
                                          const char *name, size_t name_len,
                                          const char *value, size_t value_len);
SIE_DECLARE(void) sie_xml_set_attribute(sie_XML *self, const char *name,
                                        const char *value);
SIE_DECLARE(void) sie_xml_clear_attribute_s(sie_XML *self, sie_String *name);
SIE_DECLARE(void) sie_xml_clear_attribute_b(sie_XML *self,
                                            const char *name, size_t name_len);
SIE_DECLARE(void) sie_xml_clear_attribute(sie_XML *self, const char *name);
SIE_DECLARE(sie_String *) sie_xml_get_attribute_s(sie_XML *self,
                                                  sie_String *name);
#define sie_xml_get_attribute_literal(self, literal)             \
    sie_xml_get_attribute_s(self, sie_literal(self, literal))
SIE_DECLARE(int) sie_xml_get_attribute_b(sie_XML *self,
                                         const char *name, size_t name_len,
                                         const char **dest, size_t *dest_len);
SIE_DECLARE(const char *) sie_xml_get_attribute(sie_XML *self,
                                                const char *name);
SIE_DECLARE(void) sie_xml_set_attributes(sie_XML *self, sie_XML *other);
SIE_DECLARE(int) sie_xml_attribute_equal_s(sie_XML *self, sie_XML *other,
                                           sie_String *name);
SIE_DECLARE(int) sie_xml_attribute_equal_b(sie_XML *self, sie_XML *other,
                                           const char *name, size_t name_len);
SIE_DECLARE(int) sie_xml_attribute_equal(sie_XML *self, sie_XML *other,
                                         const char *name);
SIE_DECLARE(void) sie_xml_set_name_s(sie_XML *self, sie_String *name);
SIE_DECLARE(void) sie_xml_set_name_b(sie_XML *self,
                                     const char *name, size_t name_len);
SIE_DECLARE(void) sie_xml_set_name(sie_XML *self, const char *name);
SIE_DECLARE(int) sie_xml_name_equal(sie_XML *self, sie_XML *other);

SIE_DECLARE(sie_XML_Element *) sie_xml_element(sie_XML *self);
SIE_DECLARE(sie_XML *) sie_xml_link(sie_XML *self, sie_XML *parent,
                                    enum sie_XML_Link_Type before_or_after,
                                    sie_XML *target);
SIE_DECLARE(sie_XML *) sie_xml_link_end(sie_XML *self, sie_XML *parent);
SIE_DECLARE(sie_XML *) sie_xml_insert(sie_XML *self, sie_XML *parent,
                                      enum sie_XML_Link_Type before_or_after,
                                      sie_XML *target);
SIE_DECLARE(sie_XML *) sie_xml_append(sie_XML *self, sie_XML *parent);
SIE_DECLARE(void) sie_xml_unlink(sie_XML *self);
SIE_DECLARE(sie_XML *) sie_xml_extract(sie_XML *self);

SIE_DECLARE(sie_XML *) sie_xml_new_element_s(void *ctx_obj, sie_String *name);
SIE_DECLARE(sie_XML *) sie_xml_new_element_b(void *ctx_obj, const char *name,
                                             size_t name_len);
SIE_DECLARE(sie_XML *) sie_xml_new_element(void *ctx_obj, const char *name);
SIE_DECLARE(sie_XML *) sie_xml_new_text_s(void *ctx_obj, sie_String *text);
SIE_DECLARE(sie_XML *) sie_xml_new_text_b(void *ctx_obj, const char *text,
                                          size_t text_len);
SIE_DECLARE(sie_XML *) sie_xml_new_text(void *ctx_obj, const char *text);

enum sie_XML_Descend_Type {
    SIE_XML_NO_DESCEND = 0,
    SIE_XML_DESCEND,
    SIE_XML_DESCEND_ONCE,
};
typedef int (sie_XML_Match_Fn)(sie_XML *node, void *data);

SIE_DECLARE(sie_XML *) sie_xml_walk_next(sie_XML *self, sie_XML *top,
                                         enum sie_XML_Descend_Type descend);
SIE_DECLARE(sie_XML *) sie_xml_walk_prev(sie_XML *self, sie_XML *top,
                                         enum sie_XML_Descend_Type descend);
SIE_DECLARE(sie_XML *) sie_xml_find(sie_XML *self, sie_XML *top,
                                    sie_XML_Match_Fn *match, void *data,
                                    enum sie_XML_Descend_Type descend);
SIE_DECLARE(sie_XML *) sie_xml_find_element_s(
    sie_XML *self, sie_XML *top,
    sie_String *name, sie_String *attr, sie_String *value,
    enum sie_XML_Descend_Type descend);
SIE_DECLARE(sie_XML *) sie_xml_find_element_b(
    sie_XML *self, sie_XML *top,
    const char *name, size_t name_len,
    const char *attr, size_t attr_len,
    const char *value, size_t value_len,
    enum sie_XML_Descend_Type descend);
SIE_DECLARE(sie_XML *) sie_xml_find_element(sie_XML *self, sie_XML *top,
                                            const char *name,
                                            const char *attr,
                                            const char *value,
                                            enum sie_XML_Descend_Type descend);

SIE_DECLARE(void) sie_xml_output(sie_XML *node, char **vec, int indent);
SIE_DECLARE(void) sie_xml_print(sie_XML *node);

enum sie_XML_State {
    SIE_XML_STATE_GET_TEXT = 0,
    SIE_XML_STATE_GET_ELEMENT_NAME,
    SIE_XML_STATE_GET_COMMENT,
    SIE_XML_STATE_GET_PROCESSING_INSTRUCTION,
    SIE_XML_STATE_GOT_ELEMENT_NAME,
    SIE_XML_STATE_MAYBE_GET_ATTRIBUTE,
    SIE_XML_STATE_ENSURE_EMPTY_TAG,
    SIE_XML_STATE_GET_ATTRIBUTE_EQUALS,
    SIE_XML_STATE_GET_ATTRIBUTE_EQUALS_2,
    SIE_XML_STATE_GET_ATTRIBUTE_VALUE,
    SIE_XML_STATE_GET_ATTRIBUTE_VALUE_2,
    SIE_XML_STATE_GOT_ATTRIBUTE_VALUE,
    
    SIE_XML_STATE_GOT_CLOSING_ELEMENT_NAME,
    SIE_XML_STATE_GOT_CLOSING_ELEMENT_NAME_2,
    
    SIE_XML_STATE_GET_NAME,
    SIE_XML_STATE_GET_NAME_2,
    
    SIE_XML_STATE_EAT_WHITESPACE,
    
    SIE_XML_STATE_GET_ENTITY,
    SIE_XML_STATE_GOT_ENTITY,
};

typedef void (sie_XML_Element_Started_Fn)(sie_XML *node, sie_XML *parent,
                                          int level, void *data);
typedef int (sie_XML_Text_Process_Fn)(enum sie_XML_Type type,
                                      const char *text, size_t text_size,
                                      sie_XML *parent, int level, void *data);
typedef int (sie_XML_Element_Complete_Fn)(sie_XML *node, sie_XML *parent,
                                          int level, void *data);

struct _sie_XML_Incremental_Parser {
    sie_Context_Object parent;
    enum sie_XML_State state;
    enum sie_XML_State return_state;
    char *buf;
    char *entity_buf;
    sie_String *attr_name;
    char quote;
    sie_XML **node_stack;
    sie_XML_Element_Started_Fn *element_started;
    sie_XML_Text_Process_Fn *text_process;
    sie_XML_Element_Complete_Fn *element_complete;
    void *callback_data;
};
SIE_CLASS_DECL(sie_XML_Incremental_Parser);
#define SIE_XML_INCREMENTAL_PARSER(p)                   \
    SIE_SAFE_CAST(p, sie_XML_Incremental_Parser)

SIE_DECLARE(sie_XML_Incremental_Parser *) sie_xml_incremental_parser_new(
    void *ctx_obj, sie_XML_Element_Started_Fn *element_started_fn,
    sie_XML_Text_Process_Fn *text_process_fn,
    sie_XML_Element_Complete_Fn *element_complete_fn,
    void *callback_data);
SIE_DECLARE(void) sie_xml_incremental_parser_init(
    sie_XML_Incremental_Parser *self, void *ctx_obj,
    sie_XML_Element_Started_Fn *element_started_fn,
    sie_XML_Text_Process_Fn *text_process_fn,
    sie_XML_Element_Complete_Fn *element_complete_fn,
    void *callback_data);
SIE_DECLARE(void) sie_xml_incremental_parser_destroy(
    sie_XML_Incremental_Parser *self);

SIE_DECLARE(void) sie_xml_incremental_parser_parse(
    sie_XML_Incremental_Parser *self, const char *data, size_t data_size);

struct _sie_XML_Parser {
    sie_Context_Object parent;
    sie_XML_Incremental_Parser *incremental_parser;
    sie_XML *document;
};
SIE_CLASS_DECL(sie_XML_Parser);
#define SIE_XML_PARSER(p) SIE_SAFE_CAST(p, sie_XML_Parser)

SIE_DECLARE(sie_XML_Parser *) sie_xml_parser_new(void *ctx_obj);
SIE_DECLARE(void) sie_xml_parser_init(sie_XML_Parser *self, void *ctx_obj);
SIE_DECLARE(void) sie_xml_parser_destroy(sie_XML_Parser *self);

SIE_DECLARE(void) sie_xml_parser_parse(sie_XML_Parser *self,
                                 const char *data, size_t data_size);
SIE_DECLARE(sie_XML *) sie_xml_parser_get_document(sie_XML_Parser *self);

SIE_DECLARE(sie_XML *) sie_xml_parse_string(void *ctx_obj, const char *string);


struct _sie_XML_Pack_Head {
    sie_XML parent;
    size_t pack_size;
};
SIE_CLASS_DECL(sie_XML_Pack_Head);
#define SIE_XML_PACK_HEAD(p) SIE_SAFE_CAST(p, sie_XML_Pack_Head)

struct _sie_XML_Pack_Tail {
    sie_XML parent;
};
SIE_CLASS_DECL(sie_XML_Pack_Tail);
#define SIE_XML_PACK_TAIL(p) SIE_SAFE_CAST(p, sie_XML_Pack_Tail)

SIE_DECLARE(sie_XML *) sie_xml_pack(sie_XML *tree);

#endif

#endif
