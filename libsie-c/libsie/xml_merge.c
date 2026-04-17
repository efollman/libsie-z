/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#include "sie_config.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "sie_utils.h"
#include "sie_xml.h"
#include "sie_xml_merge.h"
#include "sie_id_map.h"
#include "sie_vec.h"
#include "sie_debug.h"

#if 0
static int string_in(const char *needle, ...)
{
    va_list args;
    char *haystack;
    int retval = 0;
    
    va_start(args, needle);
    while ( (haystack = va_arg(args, char *)) ) {
        if (!strcmp(needle, haystack)) {
            retval = 1;
            break;
        }
    }
    va_end(args);

    return retval;
}
#endif

#define string_in_1(name, a) (name == a)
#define string_in_2(name, a, b) (name == a || name == b)
#define string_in_3(name, a, b, c) (name == a || name == b || name == c)

enum sie_merge_style { SIE_NOT_EQUAL = 0, SIE_REPLACE, SIE_MERGE };

static enum sie_merge_style equal_for_merge(sie_XML_Definition *xml,
                                            sie_XML *a, sie_XML *b)
{
    sie_String *name;
    if (!a || !b || !(a->type == SIE_XML_ELEMENT &&
                      b->type == SIE_XML_ELEMENT))
        return SIE_NOT_EQUAL;
    if (!sie_xml_name_equal(a, b))
        return SIE_NOT_EQUAL;
    name = a->value.element.name;
    if (string_in_3(name, xml->data, xml->xform, xml->units) ||
        (name == xml->tag && sie_xml_attribute_equal_s(a, b, xml->id)))
        return SIE_REPLACE;
    if ((string_in_2(name, xml->ch, xml->test) &&
         sie_xml_attribute_equal_s(a, b, xml->id)) ||
        (name == xml->dim &&
         sie_xml_attribute_equal_s(a, b, xml->index)))
        return SIE_MERGE;
    return SIE_NOT_EQUAL;
}

static sie_Id_Map *id_map(sie_XML_Definition *xml, sie_String *name)
{
    if (name == xml->ch) return xml->channel_map;
    if (name == xml->test) return xml->test_map;
    return NULL;
}

static int id_node_p(sie_XML_Definition *xml, sie_XML *node)
{
    return (node->type == SIE_XML_ELEMENT &&
            string_in_2(node->value.element.name, xml->ch, xml->test) &&
            sie_xml_get_attribute_s(node, xml->id) && 1);
}

static void merge_elements(sie_XML_Definition *xml,
                           sie_XML *base, sie_XML *patch, int expansion);
static sie_XML *merge_tree(sie_XML_Definition *xml, sie_XML *tree,
                           int expansion);

static sie_XML *maybe_store(sie_XML_Definition *xml, sie_XML *node,
                            int expansion)
{
    sie_Id_Map *map;
    int id;
    const char *id_s;
    
    if (expansion || !id_node_p(xml, node)) return node;
    if (!xml->any_private_attrs) {
        if (sie_xml_get_attribute_s(node, xml->private_))
            xml->any_private_attrs = 1;
    }
    id_s = sie_string_value(sie_xml_get_attribute_s(node, xml->id));
    id = sie_strtoid(xml, id_s);
    map = id_map(xml, node->value.element.name);
    sie_id_map_set(map, id, node);
    return node;
}

#define maybe_wrap(var, elem_name, var_att)                             \
    if (var) {                                                          \
        sie_XML *wrapper = sie_xml_new_element_s(node, elem_name);      \
        sie_xml_set_attribute_s(wrapper, var_att, var);                 \
        if (node->parent) {                                             \
            sie_xml_insert(wrapper, node->parent,                       \
                           SIE_XML_LINK_AFTER, node);                   \
            sie_xml_extract(node);                                      \
        }                                                               \
        sie_xml_append(node, wrapper);                                  \
        sie_xml_clear_attribute_s(original_node, elem_name);            \
        node = wrapper;                                                 \
    }

static sie_XML *maybe_expand_ch_dim_path(sie_XML_Definition *self,
                                         sie_XML *node)
{
    sie_XML *original_node = node;
    if (node->type == SIE_XML_ELEMENT) {
        sie_String *test_s = sie_xml_get_attribute_s(node, self->test);
        sie_String *ch_s = sie_xml_get_attribute_s(node, self->ch);
        sie_String *dim_s = sie_xml_get_attribute_s(node, self->dim);
        maybe_wrap(dim_s, self->dim, self->index);
        maybe_wrap(ch_s, self->ch, self->id);
        maybe_wrap(test_s, self->test, self->id);
    }
    return node;
}

static struct {
    char *old_name;
    char *new_name;
} old_new_name_translations[] = {
    { "channel", "ch" },
    { "dimension", "dim" },
    { "transform", "xform" },
};
static const int num_old_new_name_translations =
    sizeof(old_new_name_translations) / sizeof(*old_new_name_translations);

static sie_XML *merge_element_into(sie_XML_Definition *xml,
                                   sie_XML *top, sie_XML *patch,
                                   int expansion)
{
    sie_XML *base = NULL, *cur;
    sie_XML *ret;
    int style = 0;
    int i;
    const char *patch_name = sie_string_value(patch->value.element.name);

    if (!expansion) {
        sie_XML *old_patch = patch;
        if (patch->type == SIE_XML_ELEMENT) {
            /* KLUDGE - old XML syntax fixups */
            for (i = 0; i < num_old_new_name_translations; i++) {
                if (!strcmp(patch_name,
                            old_new_name_translations[i].old_name)) {
                    sie_xml_set_name(patch,
                                     old_new_name_translations[i].new_name);
                    break;
                }
            }
        }
    
        patch = maybe_expand_ch_dim_path(xml, patch);
        if (patch != old_patch)
            patch_name = sie_string_value(patch->value.element.name);

        if (patch->parent)
            sie_xml_extract(patch);
        sie_autorelease(patch);
    }

    /* A faster path for ch/test while merging (NOT expansion) */
    if (!expansion && (patch->type == SIE_XML_ELEMENT &&
                       string_in_2(patch->value.element.name,
                                   xml->ch, xml->test))) {
        const char *id_s =
            sie_string_value(sie_xml_get_attribute_s(patch, xml->id));
        sie_XML *maybe_base;
        sie_assertf(id_s, (xml, "%s element without an id.", patch_name));
        maybe_base = sie_id_map_get(id_map(xml, patch->value.element.name),
                                    sie_strtoid(xml, id_s));
        if (maybe_base) {
            sie_assertf(maybe_base->parent == top,
                        (xml, "Inconsistently placed %s element, id %s.",
                         patch_name, id_s));
            base = maybe_base;
            style = SIE_MERGE;
        }
    } else {
        for (cur = top->child; cur != NULL; cur = cur->next) {
            if ( (style = equal_for_merge(xml, cur, patch)) ) {
                base = cur;
                break;
            }
        }
    }
    
    if (!base) {
        sie_XML *merged = patch;
        if (string_in_3(patch->value.element.name,
                        xml->ch, xml->test, xml->dim))
            merged = merge_tree(xml, patch, expansion);
        else if (expansion)
            merged = sie_copy(merged);
        else
            sie_retain(merged);
        sie_xml_append(merged, top);
        ret = merged;
    } else {
        switch (style) {
        case SIE_MERGE:
            merge_elements(xml, base, patch, expansion);
            ret = base;
            break;
        case SIE_REPLACE:
            if (expansion)
                patch = sie_copy(patch);
            else
                sie_retain(patch);
            sie_xml_insert(patch, top, SIE_XML_LINK_AFTER, base);
            sie_xml_unlink(base);
            ret = patch;
            break;
        default:
            sie_errorf((xml, "impossible merge style"));
            return NULL;
        }
    }
    if (!expansion)
        sie_cleanup_pop(xml, patch, 1);
    return ret;
}

static void merge_elements(sie_XML_Definition *xml,
                           sie_XML *base, sie_XML *patch, int expansion)
/* Destructively modifies base */
{
    sie_XML *cur, *next = NULL;
    sie_xml_set_attributes(base, patch);
    if ((cur = patch->child))
        next = cur->next;
    while (cur != NULL) {
        merge_element_into(xml, base, cur, expansion);
        if ((cur = next))
            next = cur->next;
    }
}

static sie_XML *merge_tree(sie_XML_Definition *xml, sie_XML *tree,
                           int expansion)
{
    sie_XML *new_node;
    switch (tree->type) {
    case SIE_XML_ELEMENT:
        new_node = sie_xml_new_element_s(tree, tree->value.element.name);
        sie_autorelease(new_node);
        merge_elements(xml, new_node, tree, expansion);
        sie_cleanup_pop(new_node, new_node, 0);
        break;
    default:
        new_node = tree;
        break;
    }
    return maybe_store(xml, new_node, expansion);
}

SIE_CONTEXT_OBJECT_NEW_FN(sie_xml_definition_new, sie_XML_Definition,
                          self, ctx_obj, (void *ctx_obj),
                          sie_xml_definition_init(self, ctx_obj));

static void started(sie_XML *node, sie_XML *parent, int level, void *v_self)
{
    sie_XML_Definition *self = v_self;
    if (!self->sie_node_started) {
        if ((node->type == SIE_XML_ELEMENT &&
             node->value.element.name == self->sie)) {
            self->sie_node = sie_copy(node);
            /* KLUDGE read out version here */
            self->sie_node_started = 1;
        }
    }
}

static int text(enum sie_XML_Type type,
                const char *text, size_t text_size,
                sie_XML *parent, int level, void *v_self)
{
    sie_XML_Definition *self = v_self;
    if (type == SIE_XML_TEXT) {
        /* Only textual context allowed is in tags - strip
         * everything else for efficiency. */
        if (parent && parent->value.element.name != self->tag)
            return 0;
    }
    return 1;
}

static int complete(sie_XML *node, sie_XML *parent, int level, void *v_self)
{
    sie_XML_Definition *self = v_self;
    if (self->sie_node_started) {
        switch (node->type) {
        case SIE_XML_ELEMENT:
            sie_debug((self, 14, "XML Element '%s' complete, level %d.\n",
                       sie_string_value(node->value.element.name), level));
            if (level > 1) {
                return 1;
            } else if (level == 1) {
                /* "Top level" mergable thing.  Merge it into the
                 * sie_node, then do a KLUDGEy check on the resultant
                 * merged node to see if it's a decoder, and if so to
                 * add it to the decoder map. */
                sie_XML *merged;
                sie_retain(node);
                merged = merge_element_into(self, self->sie_node, node, 0);
                if (merged->value.element.name == self->decoder) {
                    const char *id_s =
                        sie_string_value(sie_xml_get_attribute_s(merged,
                                                                 self->id));
                    sie_id id;
                    sie_XML *old_node;
                    sie_assert(id_s, self);
                    id = sie_strtoid(self, id_s);
                    old_node = sie_id_map_get(self->decoder_map, id);
                    if (old_node && old_node != merged) {
                        sie_Decoder *old_decoder =
                            sie_id_map_get(self->compiled_decoder_map, id);
                        sie_xml_unlink(old_node);
                        sie_release(old_decoder);
                        sie_id_map_set(self->compiled_decoder_map, id, 0);
                    }
                    sie_debug((self, 4, "Got decoder node %p.\n", merged));
                    sie_id_map_set(self->decoder_map, id, merged);
                    sie_error_context_push(self, "Compiling decoder id '%d'",
                                           id);
                    sie_id_map_set(self->compiled_decoder_map, id,
                                   sie_decoder_new(self, merged));
                    sie_error_context_pop(self);
                }
                return 0;
            } else {
                /* Top level SIE node closing, toss it. */
                return 0;
            }
            break;
        default:
            return 1;
        }
    }
    return 0;
}

void sie_xml_definition_init(sie_XML_Definition *self, void *ctx_obj)
{
    sie_context_object_init(SIE_CONTEXT_OBJECT(self), ctx_obj);

    self->parser =
        sie_xml_incremental_parser_new(self, started, text, complete, self);
    self->channel_map = sie_id_map_new(self, 32);
    self->test_map = sie_id_map_new(self, 32);
    self->decoder_map = sie_id_map_new(self, 32);
    self->compiled_decoder_map = sie_id_map_new(self, 32);

    self->base = sie_literal(self, base);
    self->ch = sie_literal(self, ch);
    self->data = sie_literal(self, data);
    self->decoder = sie_literal(self, decoder);
    self->dim = sie_literal(self, dim);
    self->id = sie_literal(self, id);
    self->index = sie_literal(self, index);
    self->private_ = sie_literal(self, private);
    self->sie = sie_literal(self, sie);
    self->tag = sie_literal(self, tag);
    self->test = sie_literal(self, test);
    self->units = sie_literal(self, units);
    self->xform = sie_literal(self, xform);
}

void sie_xml_definition_destroy(sie_XML_Definition *self)
{
    sie_release(self->sie_node);
    if (self->compiled_decoder_map)
        sie_id_map_foreach(self->compiled_decoder_map,
                           sie_id_map_foreach_release, NULL);
    sie_release(self->compiled_decoder_map);
    sie_release(self->decoder_map);
    sie_release(self->channel_map);
    sie_release(self->test_map);
    sie_release(self->parser);
    sie_context_object_destroy(SIE_CONTEXT_OBJECT(self));
}

void sie_xml_definition_add_string(sie_XML_Definition *self,
                                   const char *string, size_t size)
{
    sie_xml_incremental_parser_parse(self->parser, string, size);
}

SIE_CLASS(sie_XML_Definition, sie_Context_Object,
          SIE_MDEF(sie_copy, sie_copy_not_applicable)
          SIE_MDEF(sie_destroy, sie_xml_definition_destroy));

static sie_XML *_sie_xml_expand(sie_XML_Definition *xml, sie_String *type,
                                sie_id id, int level)
{
    size_t mark = sie_cleanup_mark(xml);
    sie_XML *node;
    sie_String *base_s;
    sie_Id_Map *map;

    sie_recursion_limit(xml);
    map = id_map(xml, type);
    sie_assert(map, xml);
    node = sie_id_map_get(map, id);
    sie_assert(node, xml);
    base_s = sie_xml_get_attribute_s(node, xml->base);
    if (base_s) {
        sie_XML *new_node;
        sie_id base = sie_strtoid(xml, sie_sv(base_s));
        sie_assert(base >= 0, xml);
        new_node = _sie_xml_expand(xml, type, base, level + 1);
        sie_xml_clear_attribute_s(new_node, xml->private_);
        merge_elements(xml, new_node, node, 1);
        node = new_node;
    } else {
        if (level == 0)
            node = sie_retain(node);
        else
            node = sie_xml_pack(node);
    }
    sie_cleanup_pop_mark(xml, mark);
    return node;
}

sie_XML *sie_xml_expand(sie_XML_Definition *xml, sie_String *type, sie_id id)
{
    return _sie_xml_expand(xml, type, id, 0);
}
