/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#include "sie_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sie_xml.h"
#include "sie_vec.h"
#include "sie_utils.h"

SIE_CONTEXT_OBJECT_NEW_FN(sie_xml_new, sie_XML, self, ctx_obj,
                          (void *ctx_obj), sie_xml_init(self, ctx_obj));

void sie_xml_init(sie_XML *self, void *ctx_obj)
{
    sie_context_object_init(SIE_CONTEXT_OBJECT(self), ctx_obj);
}

SIE_CLASS(sie_XML, sie_Context_Object,
          SIE_MDEF(sie_copy, sie_xml_copy)
          SIE_MDEF(sie_destroy, sie_xml_destroy));

static void link_node(sie_XML *self, sie_XML *parent,
                      enum sie_XML_Link_Type before_or_after,
                      sie_XML *target)
{
    sie_assert(!self->parent && !self->next && !self->prev, self);
    self->parent = parent;
    if (!target) {
        switch (before_or_after) {
        case SIE_XML_LINK_AFTER:
            self->prev = parent->last_child;
            if (!parent->child)
                parent->child = self;
            if (parent->last_child)
                parent->last_child->next = self;
            parent->last_child = self;
            break;
        case SIE_XML_LINK_BEFORE:
            self->next = parent->child;
            if (!parent->last_child)
                parent->last_child = self;
            if (parent->child)
                parent->child->prev = self;
            parent->child = self;
            break;
        }
    } else {
        sie_assert(target->parent == parent, self);
        switch (before_or_after) {
        case SIE_XML_LINK_AFTER:
            if (parent->last_child == target)
                parent->last_child = self;
            self->prev = target;
            if (target->next)
                target->next->prev = self;
            self->next = target->next;
            target->next = self;
            break;
        case SIE_XML_LINK_BEFORE:
            if (parent->child == target)
                parent->child = self;
            self->next = target;
            if (target->prev)
                target->prev->next = self;
            self->prev = target->prev;
            target->prev = self;
            break;
        }
    }
}

static void unlink_node(sie_XML *self)
{
    sie_assert(self->parent, self);
    if (self->prev)
        self->prev->next = self->next;
    if (self->next)
        self->next->prev = self->prev;
    if (self->parent->child == self)
        self->parent->child = self->next;
    if (self->parent->last_child == self)
        self->parent->last_child = self->prev;
    self->parent = self->prev = self->next = NULL;
}

sie_XML *sie_xml_link(sie_XML *self, sie_XML *parent,
                      enum sie_XML_Link_Type before_or_after,
                      sie_XML *target)
{
    sie_retain(self);
    link_node(self, parent, before_or_after, target);
    return self;
}

sie_XML *sie_xml_link_end(sie_XML *self, sie_XML *parent)
{
    return sie_xml_link(self, parent, SIE_XML_LINK_AFTER, NULL);
}

sie_XML *sie_xml_insert(sie_XML *self, sie_XML *parent,
                        enum sie_XML_Link_Type before_or_after,
                        sie_XML *target)
{
    link_node(self, parent, before_or_after, target);
    return self;
}

sie_XML *sie_xml_append(sie_XML *self, sie_XML *parent)
{
    return sie_xml_insert(self, parent, SIE_XML_LINK_AFTER, NULL);
}

void sie_xml_unlink(sie_XML *self)
{
    if (self->parent)
        unlink_node(self);
    sie_release(self);
}

sie_XML *sie_xml_extract(sie_XML *self)
{
    unlink_node(self);
    return self;
}

void *sie_xml_copy(sie_XML *self)
{
    sie_XML *copy = sie_context_object_copy(SIE_CONTEXT_OBJECT(self));
    sie_XML *cur;
    size_t i;
    copy->parent = copy->next = copy->prev =
        copy->child = copy->last_child = NULL;
    switch (copy->type) {
    case SIE_XML_TEXT:
    case SIE_XML_COMMENT:
    case SIE_XML_PROCESSING_INSTRUCTION:
        sie_retain(copy->value.text.text);
        break;
    case SIE_XML_ELEMENT:
        sie_retain(copy->value.element.name);
        if (self->value.element.attrs == self->value.element.static_attrs) {
            copy->value.element.attrs = copy->value.element.static_attrs;
        } else {
            copy->value.element.attrs =
                sie_malloc(self, self->value.element.num_attrs *
                           sizeof(*copy->value.element.attrs));
            memcpy(copy->value.element.attrs, self->value.element.attrs,
                   sizeof(*copy->value.element.attrs) *
                   copy->value.element.num_attrs);
         }
        for (i = 0; i < copy->value.element.num_attrs; i++) {
            sie_retain(copy->value.element.attrs[i].name);
            sie_retain(copy->value.element.attrs[i].value);
        }
        break;
    }
    for (cur = self->child; cur != NULL; cur = cur->next)
        link_node(sie_copy(cur), copy, SIE_XML_LINK_AFTER, NULL);
    return copy;
}

void sie_xml_destroy(sie_XML *self)
{
    sie_XML *cur, *next;
    sie_assert(!(self->parent || self->prev || self->next), self);
    switch (self->type) {
    case SIE_XML_TEXT:
    case SIE_XML_COMMENT:
    case SIE_XML_PROCESSING_INSTRUCTION:
        sie_release(self->value.text.text);
        break;
    case SIE_XML_ELEMENT: {
        size_t i;
        for (i = 0; i < self->value.element.num_attrs; i++) {
            sie_XML_Attr *attr = &self->value.element.attrs[i];
            sie_release(attr->name);
            sie_release(attr->value);
        }
        if (self->value.element.attrs != self->value.element.static_attrs)
            free(self->value.element.attrs);
        sie_release(self->value.element.name);
        break;
    }
    }
    for (cur = self->child; cur != NULL; cur = next) {
        next = cur->next;
        sie_xml_unlink(cur);
    }
    sie_context_object_destroy(SIE_CONTEXT_OBJECT(self));
}

sie_XML_Element *sie_xml_element(sie_XML *self)
{
    sie_assert(self->type == SIE_XML_ELEMENT, self);
    return &self->value.element;
}

static sie_XML_Attr *find_attribute(sie_XML_Element *element,
                                    sie_String *name)
{
    size_t i;
    for (i = 0; i < element->num_attrs; i++) {
        if (element->attrs[i].name == name)
            return &element->attrs[i];
    }
    return NULL;
}

static void _sie_xml_set_attribute_s(sie_XML *self,
                                     sie_String *name, sie_String *value)
{
    sie_XML_Element *element = sie_xml_element(self);
    sie_XML_Attr *attr;
    if ((attr = find_attribute(element, name))) {
        sie_release(attr->value);
        attr->value = value;
        sie_release(name);
        return;
    }
    if (element->num_attrs >= (sizeof(element->static_attrs) /
                               sizeof(*element->static_attrs))) {
        if (element->attrs == element->static_attrs) {
            element->attrs = sie_malloc(self, sizeof(*element->attrs) *
                                        (element->num_attrs + 1));
            memcpy(element->attrs, element->static_attrs,
                   sizeof(element->static_attrs));
        } else {
            element->attrs =
                sie_realloc(self, element->attrs,
                            sizeof(*element->attrs) *
                            (element->num_attrs + 1));
        }
    }
    attr = &element->attrs[element->num_attrs++];
    attr->name = name;
    attr->value = value;
}

void sie_xml_set_attribute_s(sie_XML *self,
                             sie_String *name, sie_String *value)
{
    _sie_xml_set_attribute_s(self, sie_retain(name), sie_retain(value));
}

void sie_xml_set_attribute_b(sie_XML *self,
                             const char *name, size_t name_len,
                             const char *value, size_t value_len)
{
    _sie_xml_set_attribute_s(self,
                             sie_string_get(self, name, name_len),
                             sie_string_get(self, value, value_len));
}

void sie_xml_set_attribute(sie_XML *self, const char *name, const char *value)
{
    sie_xml_set_attribute_b(self, name, strlen(name),
                            value, strlen(value));
}

void sie_xml_clear_attribute_s(sie_XML *self, sie_String *name)
{
    sie_XML_Element *element = sie_xml_element(self);
    sie_XML_Attr *attr = find_attribute(element, name);
    if (attr) {
        sie_release(attr->name);
        sie_release(attr->value);
        attr->name = NULL;
        attr->value = NULL;
    }
}

void sie_xml_clear_attribute_b(sie_XML *self,
                               const char *name, size_t name_len)
{
    sie_String *ns = sie_string_get(self, name, name_len);
    sie_xml_clear_attribute_s(self, ns);
    sie_release(ns);
}

void sie_xml_clear_attribute(sie_XML *self, const char *name)
{
    sie_xml_clear_attribute_b(self, name, strlen(name));
}

sie_String *sie_xml_get_attribute_s(sie_XML *self, sie_String *name)
{
    sie_XML_Element *element = sie_xml_element(self);
    sie_XML_Attr *attr = find_attribute(element, name);
    if (attr) {
        return attr->value;
    } else {
        return NULL;
    }
}

int sie_xml_get_attribute_b(sie_XML *self,
                            const char *name, size_t name_len,
                            const char **dest, size_t *dest_len)
{
    sie_String *ns = sie_string_get(self, name, name_len);
    sie_String *value = sie_xml_get_attribute_s(self, ns);
    sie_release(ns);
    if (value) {
        *dest = sie_string_value(value);
        *dest_len = sie_string_length(value);
        return 1;
    } else {
        return 0;
    }
}

const char *sie_xml_get_attribute(sie_XML *self, const char *name)
{
    const char *retval;
    size_t retval_len;
    int got = sie_xml_get_attribute_b(self, name, strlen(name),
                                      &retval, &retval_len);
    return got ? retval : NULL;
}

void sie_xml_set_attributes(sie_XML *self, sie_XML *other)
{
    size_t i;
    for (i = 0; i < other->value.element.num_attrs; i++)
        if (other->value.element.attrs[i].name)
            sie_xml_set_attribute_s(self,
                                    other->value.element.attrs[i].name,
                                    other->value.element.attrs[i].value);
}

int sie_xml_attribute_equal_s(sie_XML *self, sie_XML *other,
                              sie_String *name)
{
    sie_String *mine = sie_xml_get_attribute_s(self, name);
    sie_String *theirs = sie_xml_get_attribute_s(other, name);
    return (mine && mine == theirs);
}

int sie_xml_attribute_equal_b(sie_XML *self, sie_XML *other,
                              const char *name, size_t name_len)
{
    sie_String *ns = sie_string_get(self, name, name_len);
    int equal = sie_xml_attribute_equal_s(self, other, ns);
    sie_release(ns);
    return equal;
}

int sie_xml_attribute_equal(sie_XML *self, sie_XML *other, const char *name)
{
    return sie_xml_attribute_equal_b(self, other, name, strlen(name));
}

void sie_xml_set_name_s(sie_XML *self, sie_String *name)
{
    sie_release(self->value.element.name);
    self->value.element.name = sie_retain(name);
}

void sie_xml_set_name_b(sie_XML *self, const char *name, size_t name_len)
{
    sie_String *ns = sie_string_get(self, name, name_len);
    sie_xml_set_name_s(self, ns);
    sie_release(ns);
}

void sie_xml_set_name(sie_XML *self, const char *name)
{
    sie_xml_set_name_b(self, name, strlen(name));
}

int sie_xml_name_equal(sie_XML *self, sie_XML *other)
{
    sie_assert(self->type == SIE_XML_ELEMENT, self);
    return self->value.element.name == other->value.element.name;
}

static sie_XML *_sie_xml_new_element_s(void *ctx_obj, sie_String *name)
{
    sie_XML *self = sie_xml_new(ctx_obj);
    self->type = SIE_XML_ELEMENT;
    self->value.element.name = name;
    self->value.element.attrs = self->value.element.static_attrs;
    return self;
}

sie_XML *sie_xml_new_element_s(void *ctx_obj, sie_String *name)
{
    return _sie_xml_new_element_s(ctx_obj, sie_retain(name));
}

sie_XML *sie_xml_new_element_b(void *ctx_obj, const char *name,
                               size_t name_len)
{
    return _sie_xml_new_element_s(ctx_obj,
                                  sie_string_get(ctx_obj, name, name_len));
}

sie_XML *sie_xml_new_element(void *ctx_obj, const char *name)
{
    return sie_xml_new_element_b(ctx_obj, name, strlen(name));
}

static sie_XML *_sie_xml_new_text_s(void *ctx_obj, sie_String *text)
{
    sie_XML *self = sie_xml_new(ctx_obj);
    self->type = SIE_XML_TEXT;
    self->value.text.text = text;
    return self;
}

sie_XML *sie_xml_new_text_s(void *ctx_obj, sie_String *text)
{
    return _sie_xml_new_text_s(ctx_obj, sie_retain(text));
}

sie_XML *sie_xml_new_text_b(void *ctx_obj, const char *text,
                            size_t text_len)
{
    return _sie_xml_new_text_s(ctx_obj,
                               sie_string_get(ctx_obj, text, text_len));
}

sie_XML *sie_xml_new_text(void *ctx_obj, const char *text)
{
    return sie_xml_new_text_b(ctx_obj, text, strlen(text));
}


sie_XML *sie_xml_walk_next(sie_XML *self, sie_XML *top,
                           enum sie_XML_Descend_Type descend)
{
    if (!self) {
        return NULL;
    } else if (self->child && descend) {
        return self->child;
    } else if (self->next) {
        if (self == top)
            return NULL;
        return self->next;
    } else if (self->parent && self->parent != top) {
        self = self->parent;
        while (!self->next) {
            if (self->parent == top || !self->parent)
                return NULL;
            else
                self = self->parent;
        }
        return self->next;
    } else {
        return NULL;
    }
}

sie_XML *sie_xml_walk_prev(sie_XML *self, sie_XML *top,
                           enum sie_XML_Descend_Type descend)
{
    if (!self) {
        return NULL;
    } else if (self->prev) {
        if (self->prev->last_child && descend) {
            self = self->prev->last_child;
            while (self->last_child)
                self = self->last_child;
            return self;
        } else {
            return self->prev;
        }
    } else if (self->parent != top) {
        return self->parent;
    } else {
        return NULL;
    }
}

sie_XML *sie_xml_find(sie_XML *self, sie_XML *top,
                      sie_XML_Match_Fn *match, void *data,
                      enum sie_XML_Descend_Type descend)
{
    self = sie_xml_walk_next(self, top, descend);
    while (self != NULL) {
        if (match(self, data))
            return self;
        self = sie_xml_walk_next(self, top,
                                 (descend == SIE_XML_DESCEND_ONCE ?
                                  SIE_XML_NO_DESCEND : descend));
    }
    return NULL;
}

struct find_element_match_data {
    sie_String *name;
    sie_String *attr;
    sie_String *value;
};
    
static int find_element_match(sie_XML *node, void *data)
{
    struct find_element_match_data *md = data;
    if (node->type == SIE_XML_ELEMENT &&
        node->value.element.name &&
        (!md->name || node->value.element.name == md->name)) {
        if (!md->attr)
            return 1;
        if (sie_xml_get_attribute_s(node, md->attr) == md->value)
            return 1;
    }
    return 0;
}

sie_XML *sie_xml_find_element_s(sie_XML *self, sie_XML *top,
                                sie_String *name, sie_String *attr,
                                sie_String *value,
                                enum sie_XML_Descend_Type descend)
{
    struct find_element_match_data md = { name, attr, value };
    return sie_xml_find(self, top, find_element_match, &md, descend);
}

sie_XML *sie_xml_find_element_b(sie_XML *self, sie_XML *top,
                                const char *name, size_t name_len,
                                const char *attr, size_t attr_len,
                                const char *value, size_t value_len,
                                enum sie_XML_Descend_Type descend)
{
    sie_String *ns = sie_string_get(self, name, name_len);
    sie_String *as = sie_string_get(self, attr, attr_len);
    sie_String *vs = sie_string_get(self, value, value_len);
    sie_XML *result = sie_xml_find_element_s(self, top, ns, as, vs, descend);
    sie_release(vs);
    sie_release(as);
    sie_release(ns);
    return result;
}

sie_XML *sie_xml_find_element(sie_XML *self, sie_XML *top,
                              const char *name,
                              const char *attr,
                              const char *value,
                              enum sie_XML_Descend_Type descend)
{
    return sie_xml_find_element_b(self, top,
                                  name, name ? strlen(name) : 0,
                                  attr, attr ? strlen(attr) : 0,
                                  value, value ? strlen(value) : 0,
                                  descend);
}

static void output_quoted(void *self, const char *src, size_t len, char **vec,
                          int escape_quot)
{
    size_t last = 0;
    size_t cur;
    for (cur = 0; cur < len; ++cur) {
        char c = src[cur];
        if (c == '&' || c == '<' || (c == '"' && escape_quot)) {
            sie_vec_memcat(self, vec, &src[last], cur - last);
            switch (c) {
            case '&': sie_vec_strcatf(self, vec, "&amp;"); break;
            case '"': sie_vec_strcatf(self, vec, "&quot;"); break;
            case '<': sie_vec_strcatf(self, vec, "&lt;"); break;
            }
            last = cur + 1;
        }
    }
    sie_vec_memcat(self, vec, &src[last], cur - last);
}

static void output_quoted_s(void *self, sie_String *src, char **vec, int escape_quot)
{
    output_quoted(self, sie_string_value(src), sie_string_length(src),
                  vec, escape_quot);
}

void sie_xml_output(sie_XML *node, char **vec, int indent)
{
    int next_indent = indent + 1;
    int inner_indent;
    const char *newline = "\n";
    size_t i;
    if (!node) return;
    if (indent < 0) {
        indent = 0;
        newline = "";
        next_indent = -1;
    }
    switch (node->type) {
    case SIE_XML_ELEMENT:
        sie_vec_strcatf(node, vec, "%*s<%s", indent, "",
                        sie_string_value(node->value.element.name));
        for (i = 0; i < node->value.element.num_attrs; i++) {
            sie_XML_Attr *attr = &node->value.element.attrs[i];
            if (attr->name) {
                sie_vec_strcatf(node, vec, " %s=\"",
                                sie_string_value(attr->name));
                output_quoted_s(node, attr->value, vec, 1);
                sie_vec_strcatf(node, vec, "\"");
            }
        }
        if (node->child) {
            sie_XML *cur;
            inner_indent = next_indent;
            for (cur = node->child; cur; cur = cur->next) {
                if (cur->type == SIE_XML_TEXT) {
                    inner_indent = -1;
                    break;
                }
            }
            sie_vec_strcatf(node, vec, ">%s", inner_indent < 0 ? "" : newline);
            for (cur = node->child; cur != NULL; cur = cur->next)
                sie_xml_output(cur, vec, inner_indent);
            sie_vec_strcatf(node, vec, "%*s</%s>%s",
                            inner_indent < 0 ? 0 : indent, "",
                            sie_string_value(node->value.element.name),
                            newline);
        } else {
            sie_vec_strcatf(node, vec, "/>%s", newline);
        }
        break;
    case SIE_XML_TEXT:
        output_quoted_s(node, node->value.text.text, vec, 0);
        break;
    case SIE_XML_COMMENT:
    case SIE_XML_PROCESSING_INSTRUCTION:
        sie_vec_strcatf(node, vec, "<%s>%s", sie_string_value(node->value.text.text),
                        newline);
        break;
    }
}

void sie_xml_print(sie_XML *node)
{
    /* For debugging */
    char *buf = NULL;
    sie_xml_output(node, &buf, 0);
    printf("%s\n", buf);
    sie_vec_free(buf);
}

#define NAME_START_CHAR    0x1
#define NAME_CHAR          0x2
#define WHITESPACE         0x4
#define ATTRIBUTE_QUOTES   0x8
#define ENTITY_CHAR       0x10

static int char_table[256];
static int char_table_initialized = 0;

static void initialize_char_table(void)
{
    int ch;
    for (ch = 0; ch < 256; ch++) {
        char_table[ch] = 0;
        
        if ((ch == ':' || (ch >= 'A' && ch <= 'Z') ||
             ch == '_' || (ch >= 'a' && ch <= 'z') || ch >= 128))
            char_table[ch] |= NAME_START_CHAR | NAME_CHAR | ENTITY_CHAR;

        if ((ch == '-' || ch == '.' || (ch >= '0' && ch <= '9')))
            char_table[ch] |= NAME_CHAR | ENTITY_CHAR;
        
        if ((ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n'))
            char_table[ch] |= WHITESPACE;
        
        if ((ch == '\'' || ch == '"'))
            char_table[ch] |= ATTRIBUTE_QUOTES;

        if (ch == '#')
            char_table[ch] |= ENTITY_CHAR;
    }
    
    char_table_initialized = 1;
}

#define char_is(ch, category) (char_table[(unsigned char)(ch)] & (category))

static void maybe_notify_started(sie_XML_Incremental_Parser *self,
                                 sie_XML *node)
{
    sie_XML *parent = NULL;
    if (sie_vec_size(self->node_stack))
        parent = *sie_vec_back(self->node_stack);
    if (self->element_started)
        self->element_started(node, parent,
                              (int)sie_vec_size(self->node_stack) - 1,
                              self->callback_data);
}

static void maybe_link(sie_XML_Incremental_Parser *self,
                       sie_XML *node, enum sie_XML_Type type,
                       const char *text, size_t text_size)
{
    sie_XML *parent = NULL;
    if (sie_vec_size(self->node_stack))
        parent = *sie_vec_back(self->node_stack);
    if (type != SIE_XML_ELEMENT &&
        self->text_process &&
        !self->text_process(type, text, text_size,
                            parent, (int)sie_vec_size(self->node_stack),
                            self->callback_data))
        return;
    if (type != SIE_XML_ELEMENT) {
        node = sie_xml_new_text_b(self, text, text_size);
        node->type = type;
    }
    sie_cleanup_push(self, sie_release, node);
    if (self->element_complete &&
        self->element_complete(node, parent,
                               (int)sie_vec_size(self->node_stack),
                               self->callback_data) && parent) {
        link_node(node, parent, SIE_XML_LINK_AFTER, NULL);
        sie_cleanup_pop(self, node, 0);
    } else {
        sie_cleanup_pop(self, node, 1);
    }
}

static struct {
    char *name;
    int value;
} entities[] = {
    { "lt",   '<'  },
    { "gt",   '>'  },
    { "amp",  '&'  },
    { "apos", '\'' },
    { "quot", '"'  },
};

static int parse_entity(char *name)
{
    size_t i;
    int retval = -1;
    char *endptr = NULL;
    if (name[0] == '#') {
        if (name[1] == 'x')
            retval = name[2] ? strtol(&name[2], &endptr, 16) : -1;
        else
            retval = name[1] ? strtol(&name[1], &endptr, 10) : -1;
        if (*endptr)
            retval = -1;
    } else {
        for (i = 0; i < sizeof(entities) / sizeof(*entities); i++) {
            if (!strcmp(name, entities[i].name)) {
                retval = entities[i].value;
                break;
            }
        }
    }
    return retval;
}

void sie_xml_incremental_parser_parse(sie_XML_Incremental_Parser *self,
                                      const char *data, size_t data_size)
{
    size_t i;
    sie_XML *node;
    int entity;
    
    for (i = 0; i < data_size; i++) {
        char ch = data[i];
    restart:
        switch (self->state) {
        case SIE_XML_STATE_GET_TEXT:
            switch (ch) {
            case '<':
                if (sie_vec_size(self->buf)) {
                    maybe_link(self, NULL, SIE_XML_TEXT,
                               self->buf, sie_vec_size(self->buf));
                    sie_vec_clear(self->buf);
                }
                self->state = SIE_XML_STATE_GET_ELEMENT_NAME;
                break;
            case '&':
                self->state = SIE_XML_STATE_GET_ENTITY;
                self->return_state = SIE_XML_STATE_GET_TEXT;
                break;
            default:
                sie_vec_push_back(self->buf, ch);
            }
            break;
            
        case SIE_XML_STATE_GET_ELEMENT_NAME:
            self->state = SIE_XML_STATE_GET_NAME;
            switch (ch) {
            case '!':
                self->state = SIE_XML_STATE_GET_COMMENT;
                goto restart;
            case '?':
                self->state = SIE_XML_STATE_GET_PROCESSING_INSTRUCTION;
                goto restart;
            case '/':
                self->return_state = SIE_XML_STATE_GOT_CLOSING_ELEMENT_NAME;
                break;
            default:
                self->return_state = SIE_XML_STATE_GOT_ELEMENT_NAME;
                goto restart;
            }
            break;

        case SIE_XML_STATE_GET_COMMENT:
            if (ch == '>' && (sie_vec_size(self->buf) > 4) &&
                !strncmp(self->buf + sie_vec_size(self->buf) - 2, "--", 2) &&
                self->buf[sie_vec_size(self->buf) - 3] != '-')
            {
                maybe_link(self, NULL, SIE_XML_COMMENT,
                           self->buf, sie_vec_size(self->buf));
                sie_vec_clear(self->buf);
                self->state = SIE_XML_STATE_GET_TEXT;
                break;
            } else {
                sie_vec_push_back(self->buf, ch);
                if (sie_vec_size(self->buf) == 4)
                    sie_assert(!strncmp(self->buf, "!--", 3) &&
                               self->buf[3] != '-', self);
            }
            break;

        case SIE_XML_STATE_GET_PROCESSING_INSTRUCTION:
            if (ch == '>' && (sie_vec_size(self->buf) > 1) &&
                self->buf[sie_vec_size(self->buf) - 1] == '?')
            {
                maybe_link(self, NULL, SIE_XML_PROCESSING_INSTRUCTION,
                           self->buf, sie_vec_size(self->buf));
                sie_vec_clear(self->buf);
                self->state = SIE_XML_STATE_GET_TEXT;
                break;
            } else {
                sie_vec_push_back(self->buf, ch);
            }
            break;

        case SIE_XML_STATE_GOT_ELEMENT_NAME: 
            node = sie_xml_new_element_b(self, self->buf,
                                         sie_vec_size(self->buf));
            sie_vec_clear(self->buf);
            sie_vec_push_back(self->node_stack, node);
            self->state = SIE_XML_STATE_EAT_WHITESPACE;
            self->return_state = SIE_XML_STATE_MAYBE_GET_ATTRIBUTE;
            goto restart;
            
        case SIE_XML_STATE_MAYBE_GET_ATTRIBUTE:
            switch (ch) {
            case '/':
                self->state = SIE_XML_STATE_ENSURE_EMPTY_TAG;
                break;
            case '>':
                sie_assert(sie_vec_back(self->node_stack), self);
                node = *sie_vec_back(self->node_stack);
                sie_assert(node, self);
                maybe_notify_started(self, node);
                self->state = SIE_XML_STATE_GET_TEXT;
                break;
            default:
                self->state = SIE_XML_STATE_GET_NAME;
                self->return_state = SIE_XML_STATE_GET_ATTRIBUTE_EQUALS;
                goto restart;
            }
            break;
            
        case SIE_XML_STATE_ENSURE_EMPTY_TAG:
            sie_assert(ch == '>', self);
            self->state = SIE_XML_STATE_GOT_CLOSING_ELEMENT_NAME_2;
            goto restart;
            
        case SIE_XML_STATE_GET_ATTRIBUTE_EQUALS:
            self->attr_name = sie_string_get(self, self->buf,
                                             sie_vec_size(self->buf));
            sie_vec_clear(self->buf);
            self->state = SIE_XML_STATE_EAT_WHITESPACE;
            self->return_state = SIE_XML_STATE_GET_ATTRIBUTE_EQUALS_2;
            goto restart;
            
        case SIE_XML_STATE_GET_ATTRIBUTE_EQUALS_2:
            sie_assert(ch == '=', self);
            self->state = SIE_XML_STATE_EAT_WHITESPACE;
            self->return_state = SIE_XML_STATE_GET_ATTRIBUTE_VALUE;
            break;
            
        case SIE_XML_STATE_GET_ATTRIBUTE_VALUE:
            if (char_is(ch, ATTRIBUTE_QUOTES))
                self->quote = ch;
            else
                sie_errorf((self, "Expected quoted value."));
            self->state = SIE_XML_STATE_GET_ATTRIBUTE_VALUE_2;
            break;
            
        case SIE_XML_STATE_GET_ATTRIBUTE_VALUE_2:
            if (ch == '<') {
                sie_errorf((self, "Illegal character '%c' in attribute "
                            "value.", ch));
            } else if (ch == self->quote) {
                self->state = SIE_XML_STATE_GOT_ATTRIBUTE_VALUE;
            } else if (ch == '&') {
                self->state = SIE_XML_STATE_GET_ENTITY;
                self->return_state = SIE_XML_STATE_GET_ATTRIBUTE_VALUE_2;
            } else {
                sie_vec_push_back(self->buf, ch);
            }
            break;
            
        case SIE_XML_STATE_GOT_ATTRIBUTE_VALUE:
            sie_assert(sie_vec_back(self->node_stack), self);
            node = *sie_vec_back(self->node_stack);
            sie_assert(node, self);
            _sie_xml_set_attribute_s(node, self->attr_name,
                                     sie_string_get(self, self->buf,
                                                    sie_vec_size(self->buf)));
            self->attr_name = NULL;
            sie_vec_clear(self->buf);
            self->state = SIE_XML_STATE_EAT_WHITESPACE;
            self->return_state = SIE_XML_STATE_MAYBE_GET_ATTRIBUTE;
            goto restart;
            
        case SIE_XML_STATE_GOT_CLOSING_ELEMENT_NAME:
            sie_assertf(sie_vec_back(self->node_stack),
                        (self, "Got extra closing tag '%.*s'",
                         (int)sie_vec_size(self->buf), self->buf));
            node = *sie_vec_back(self->node_stack);
            sie_assertf(sie_string_find(self, self->buf,
                                        sie_vec_size(self->buf)) ==
                        node->value.element.name,
                        (self, "Wrong closing tag (expected '%s' got '%.*s')",
                         sie_sv(node->value.element.name),
                         (int)sie_vec_size(self->buf), self->buf));
            sie_vec_clear(self->buf);
            self->state = SIE_XML_STATE_EAT_WHITESPACE;
            self->return_state = SIE_XML_STATE_GOT_CLOSING_ELEMENT_NAME_2;
            goto restart;
            
        case SIE_XML_STATE_GOT_CLOSING_ELEMENT_NAME_2:
            sie_assert(sie_vec_back(self->node_stack), self);
            sie_assert(ch == '>', self);
            node = *sie_vec_back(self->node_stack);
            sie_vec_pop_back(self->node_stack);
            maybe_link(self, node, SIE_XML_ELEMENT, NULL, 0);
            self->state = SIE_XML_STATE_GET_TEXT;
            break;
            
        case SIE_XML_STATE_GET_NAME:
            if (char_is(ch, NAME_START_CHAR)) {
                sie_vec_push_back(self->buf, ch);
                self->state = SIE_XML_STATE_GET_NAME_2;
            } else {
                sie_errorf((self, "Badly-formed XML: illegal character "
                            "'%c' (0x%02x) as NameStartChar", ch, ch));
            }
            break;
            
        case SIE_XML_STATE_GET_NAME_2:
            if (char_is(ch, NAME_CHAR)) {
                sie_vec_push_back(self->buf, ch);
            } else {
                self->state = self->return_state;
                goto restart;
            }
            break;            

        case SIE_XML_STATE_EAT_WHITESPACE:
            if (!char_is(ch, WHITESPACE)) {
                self->state = self->return_state;
                goto restart;
            }
            break;
            
        case SIE_XML_STATE_GET_ENTITY:
            if (char_is(ch, ENTITY_CHAR)) {
                sie_vec_push_back(self->entity_buf, ch);
            } else {
                self->state = SIE_XML_STATE_GOT_ENTITY;
                goto restart;
            }
            break;

        case SIE_XML_STATE_GOT_ENTITY:
            sie_assert(ch == ';', self);
            sie_vec_push_back(self->entity_buf, 0);
            entity = parse_entity(self->entity_buf);
            if (entity < 0)
                sie_errorf((self, "Illegal entity '%s'",
                            self->entity_buf));
            sie_vec_clear(self->entity_buf);
            sie_add_char_to_utf8_vec(self, &self->buf, entity);
            self->state = self->return_state;
            break;
        }
        
    }
}

SIE_CONTEXT_OBJECT_NEW_FN(
    sie_xml_incremental_parser_new, sie_XML_Incremental_Parser, self, ctx_obj,
    (void *ctx_obj, sie_XML_Element_Started_Fn *element_started_fn,
     sie_XML_Text_Process_Fn *text_process_fn,
     sie_XML_Element_Complete_Fn *element_complete_fn,
     void *callback_data),
    sie_xml_incremental_parser_init(
        self, ctx_obj, element_started_fn,
        text_process_fn, element_complete_fn,
        callback_data));

void sie_xml_incremental_parser_init(
    sie_XML_Incremental_Parser *self, void *ctx_obj,
    sie_XML_Element_Started_Fn *element_started_fn,
    sie_XML_Text_Process_Fn *text_process_fn,
    sie_XML_Element_Complete_Fn *element_complete_fn,
    void *callback_data)
{
    sie_context_object_init(SIE_CONTEXT_OBJECT(self), ctx_obj);
    if (!char_table_initialized)
        initialize_char_table();
    self->element_started = element_started_fn;
    self->text_process = text_process_fn;
    self->element_complete = element_complete_fn;
    self->callback_data = callback_data;
}

void sie_xml_incremental_parser_destroy(sie_XML_Incremental_Parser *self)
{
    sie_XML **cur;
    sie_vec_forall(self->node_stack, cur) {
        sie_release(*cur);
    }
    sie_vec_free(self->node_stack);
    sie_vec_free(self->entity_buf);
    sie_vec_free(self->buf);
    sie_release(self->attr_name);
    sie_context_object_destroy(SIE_CONTEXT_OBJECT(self));
}

SIE_CLASS(sie_XML_Incremental_Parser, sie_Context_Object,
          SIE_MDEF(sie_copy, sie_copy_not_applicable)
          SIE_MDEF(sie_destroy, sie_xml_incremental_parser_destroy));

SIE_CONTEXT_OBJECT_NEW_FN(sie_xml_parser_new, sie_XML_Parser,
                          self, ctx_obj, (void *ctx_obj),
                          sie_xml_parser_init(self, ctx_obj));

static int node_complete(sie_XML *node, sie_XML *parent,
                         int level, void *v_self)
{
    sie_XML_Parser *self = v_self;
    if (level == 0 && node->type == SIE_XML_ELEMENT && !self->document)
        self->document = sie_retain(node);
    return 1;
}

void sie_xml_parser_init(sie_XML_Parser *self, void *ctx_obj)
{
    sie_context_object_init(SIE_CONTEXT_OBJECT(self), ctx_obj);
    self->incremental_parser =
        sie_xml_incremental_parser_new(self, NULL, NULL, node_complete, self);
}

void sie_xml_parser_destroy(sie_XML_Parser *self)
{
    sie_release(self->document);
    sie_release(self->incremental_parser);
    sie_context_object_destroy(SIE_CONTEXT_OBJECT(self));
}

void sie_xml_parser_parse(sie_XML_Parser *self, const char *data,
                          size_t data_size)
{
    sie_xml_incremental_parser_parse(self->incremental_parser,
                                     data, data_size);
}

sie_XML *sie_xml_parser_get_document(sie_XML_Parser *self)
{
    return sie_retain(self->document);
}

SIE_CLASS(sie_XML_Parser, sie_Context_Object,
          SIE_MDEF(sie_copy, sie_copy_not_applicable)
          SIE_MDEF(sie_destroy, sie_xml_parser_destroy));

sie_XML *sie_xml_parse_string(void *ctx_obj, const char *string)
{
    sie_XML_Parser *parser = sie_autorelease(sie_xml_parser_new(ctx_obj));
    sie_XML *document;
    sie_xml_parser_parse(parser, string, strlen(string));
    document = sie_xml_parser_get_document(parser);
    sie_cleanup_pop(ctx_obj, parser, 1);
    return document;
}


static void copy_node(sie_XML *to, sie_XML *from)
{
    size_t i;
    to->type = from->type;
    switch (to->type) {
    case SIE_XML_ELEMENT:
        to->value.element.name = sie_retain(from->value.element.name);
        to->value.element.num_attrs = from->value.element.num_attrs;
        if (from->value.element.attrs == from->value.element.static_attrs)
            to->value.element.attrs = to->value.element.static_attrs;
        else
            to->value.element.attrs = sie_malloc(from, to->value.element.num_attrs *
                                                 sizeof(*to->value.element.attrs));
        for (i = 0; i < to->value.element.num_attrs; i++) {
            to->value.element.attrs[i].name =
                sie_retain(from->value.element.attrs[i].name);
            to->value.element.attrs[i].value =
                sie_retain(from->value.element.attrs[i].value);
        }
        break;
    case SIE_XML_TEXT:
    case SIE_XML_COMMENT:
    case SIE_XML_PROCESSING_INSTRUCTION:
        to->value.text.text = sie_retain(from->value.text.text);
        break;
    }
}

static sie_XML *pack_node(sie_Context *ctx, sie_XML *tree,
                          sie_XML_Pack_Tail **tail)
{
    sie_XML *self = (sie_XML *)((*tail)++);
    sie_XML *cur;
    sie_alloc_in_place(SIE_CLASS_FOR_TYPE(sie_XML_Pack_Tail), self);
    sie_refcount(self) = 1;
    SIE_CONTEXT_OBJECT(self)->context = ctx;
    copy_node(self, tree);
    for (cur = tree->child; cur != NULL; cur = cur->next)
        link_node(pack_node(ctx, cur, tail), self, SIE_XML_LINK_AFTER, NULL);
    return self;
}

sie_XML *sie_xml_pack(sie_XML *tree)
{
    size_t pack_size = sizeof(sie_XML_Pack_Head);
    sie_XML *cur = tree;
    sie_XML_Pack_Head *self;
    sie_XML_Pack_Tail *tail;
    sie_Context *ctx = sie_context(tree);
    
    while ((cur = sie_xml_walk_next(cur, tree, SIE_XML_DESCEND)))
        pack_size += sizeof(sie_XML_Pack_Tail);

    self = sie_calloc(tree, pack_size);
    sie_alloc_in_place(SIE_CLASS_FOR_TYPE(sie_XML_Pack_Head), self);
    self->pack_size = pack_size;
    tail = (sie_XML_Pack_Tail *)(self + 1);
    sie_xml_init(SIE_XML(self), tree);
    copy_node(SIE_XML(self), tree);
    for (cur = tree->child; cur != NULL; cur = cur->next)
        link_node(pack_node(ctx, cur, &tail), SIE_XML(self),
                  SIE_XML_LINK_AFTER, NULL);
    return SIE_XML(self);
}

static void sie_xml_pack_head_destroy(sie_XML_Pack_Head *self)
{
    sie_XML *xml = SIE_XML(self);
    size_t num_nodes = ((self->pack_size - sizeof(sie_XML_Pack_Head)) /
                        sizeof(sie_XML_Pack_Tail));
    sie_XML_Pack_Tail *curp = num_nodes ? SIE_XML_PACK_TAIL(self + 1) : NULL;
    size_t i, j;
    sie_XML *cur, *next;

    sie_assert(!(xml->parent || xml->prev || xml->next), self);
    for (cur = xml->child; cur != NULL; cur = next) {
        next = cur->next;
        sie_xml_unlink(cur);
    }

    for (i = 0; i < num_nodes; i++) {
        cur = SIE_XML(curp);
        if (sie_refcount(cur) >= 1)
            abort();
        switch (cur->type) {
        case SIE_XML_ELEMENT:
            sie_release(cur->value.element.name);
            for (j = 0; j < cur->value.element.num_attrs; j++) {
                sie_release(cur->value.element.attrs[j].name);
                sie_release(cur->value.element.attrs[j].value);
            }
            if (cur->value.element.attrs != cur->value.element.static_attrs)
                free(cur->value.element.attrs);
            break;
        case SIE_XML_TEXT:
        case SIE_XML_COMMENT:
        case SIE_XML_PROCESSING_INSTRUCTION:
            sie_release(cur->value.text.text);
            break;
        };

        curp++;
    }

    sie_xml_destroy(SIE_XML(self));
}

SIE_CLASS(sie_XML_Pack_Head, sie_XML,
          SIE_MDEF(sie_destroy, sie_xml_pack_head_destroy));

static void sie_xml_pack_tail_destroy(sie_XML_Pack_Tail *self)
{
    sie_XML *xml = SIE_XML(self);
    sie_XML *cur, *next;
    sie_assert(!(xml->parent || xml->prev || xml->next), self);
    for (cur = xml->child; cur != NULL; cur = next) {
        next = cur->next;
        sie_xml_unlink(cur);
    }
}

static void sie_xml_pack_tail_free_object(sie_XML_Pack_Tail *self)
{
}

SIE_CLASS(sie_XML_Pack_Tail, sie_XML,
          SIE_MDEF(sie_destroy, sie_xml_pack_tail_destroy)
          SIE_MDEF(sie_free_object, sie_xml_pack_tail_free_object));
