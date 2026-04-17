/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#include "sie_config.h"

#include "sie_tag.h"
#include "sie_group.h"
#include "sie_utils.h"

void sie_tag_init(sie_Tag *self, void *intake, sie_XML *node)
{
    sie_String *group_s;

    sie_ref_init(SIE_REF(self), intake);

    self->id = sie_retain(sie_xml_get_attribute_literal(node, id));
    sie_assert(self->id, intake);

    group_s = sie_xml_get_attribute_literal(node, group);
    self->value = sie_literal(self, NULL);
    /* No child text node for <tag id="foo"></tag> */

    if (node->child && node->child->type == SIE_XML_TEXT)
        self->value = node->child->value.text.text;

    sie_retain(self->value);

    self->node = sie_retain(node);
    self->intake = sie_retain(intake);

    if (group_s)
        self->group = sie_strtoid(self, sie_sv(group_s));
    else
        self->group = SIE_NULL_ID;
}

void sie_tag_destroy(sie_Tag *self)
{
    sie_release(self->value);
    sie_release(self->id);
    sie_release(self->intake);
    sie_release(self->node);
    sie_ref_destroy(SIE_REF(self));
}

sie_Spigot *sie_tag_attach_spigot(sie_Tag *self)
{
    void *spigot;
    if (self->group != SIE_NULL_ID) {
        sie_Group *group = sie_group_new(self->intake, self->group);
        spigot = sie_group_spigot_new(group);
        sie_release(group);
    } else {
        spigot = sie_tag_spigot_new(self);
    }
    return spigot;
}

SIE_CONTEXT_OBJECT_NEW_FN(sie_tag_new, sie_Tag, self, intake,
                          (void *intake, sie_XML *node),
                          sie_tag_init(self, intake, node));

void sie_tag_dump(sie_Tag *self, FILE *stream, char *prefix)
{
    fprintf(stream, "%sTag \"%s\":", prefix, sie_string_value(self->id));
    if (self->value) {
        fprintf(stream, " \"%s\"\n", sie_string_value(self->value));
    } else {
        fprintf(stream, " external in group %d\n", self->group);
    }
}

const char *sie_tag_get_id(sie_Tag *self)
{
    if (!self) return NULL;
    return sie_string_value(self->id);
}

sie_id sie_tag_get_group(sie_Tag *self)
{
    if (!self) return SIE_NULL_ID;
    return self->group;
}

int sie_tag_is_from_group(sie_Tag *self)
{
    return !(sie_tag_get_group(self) == SIE_NULL_ID);
}

sie_uint64 sie_tag_get_value_size(sie_Tag *self)
{
    sie_uint64 volatile size = 0;
    SIE_API_WRAPPER(self, 0) {
        if (self->group == SIE_NULL_ID) {
            size = sie_string_length(self->value);
        } else {
            void *gh = sie_get_group_handle(self->intake, self->group);
            size = gh ? sie_get_group_num_bytes(self->intake, gh) : 0;
        }
    } SIE_END_API_WRAPPER(size, 0);
}

sie_String *sie_tag_get_id_s(sie_Tag *self)
{
    if (!self) return NULL;
    return self->id;
}

sie_String *sie_tag_get_value_s(sie_Tag *self)
{
    if (!self) return NULL;
    return self->value;
}

int sie_tag_get_value_b(sie_Tag *self, char **value, size_t *size)
{
    SIE_API_WRAPPER(self, 0) {
        sie_Spigot *spigot = sie_autorelease(sie_attach_spigot(self));
        sie_uint64 payload_size = sie_tag_get_value_size(self);
        size_t size_t_size = (size_t)payload_size;
        sie_Output *data;
        char *buf;
        size_t siz = 0;
        char *ptr;
        sie_assert((size_t_size + 1) == (payload_size + 1), self);
        buf = sie_cleanup_push(self, free, sie_malloc(self, size_t_size + 1));
        sie_assert(buf, self);
        buf[0] = 0;
        ptr = buf;
        while ( (data = sie_spigot_get(spigot)) ) {
            sie_assert(siz + data->v[0].raw[0].size <= size_t_size, self);
            memcpy(ptr, data->v[0].raw[0].ptr, data->v[0].raw[0].size);
            ptr += data->v[0].raw[0].size;
            siz += data->v[0].raw[0].size;
        }
        sie_assert(siz == size_t_size, self);
        buf[size_t_size] = 0;
        *value = buf;
        *size = siz;
        sie_cleanup_pop(self, buf, 0);
        sie_cleanup_pop(self, spigot, 1);
    } SIE_END_API_WRAPPER(1, 0);
}

char *sie_tag_get_value(sie_Tag *self)
{
    char *value = NULL;
    size_t size = 0;
    if (sie_tag_get_value_b(self, &value, &size))
        return value;
    else
        return NULL;
}

void *sie_tag_xml_iterator_realize(void *ctx_obj, void *v_xml, void *v_intake)
{
    return sie_tag_new(v_intake, v_xml);
}

SIE_CLASS(sie_Tag, sie_Ref,
          SIE_MDEF(sie_destroy, sie_tag_destroy)
          SIE_MDEF(sie_attach_spigot, sie_tag_attach_spigot));



void sie_tag_spigot_init(sie_Tag_Spigot *self, sie_Tag *tag)
{
    sie_spigot_init(SIE_SPIGOT(self), tag);
    self->tag = tag;
    sie_retain(self->tag);
    self->output = sie_output_new(self, 1);
    sie_output_set_type(self->output, 0, SIE_OUTPUT_RAW);
}

void sie_tag_spigot_destroy(sie_Tag_Spigot *self)
{
    sie_release(self->output);
    sie_release(self->tag);
    sie_spigot_destroy(SIE_SPIGOT(self));
}

SIE_CONTEXT_OBJECT_NEW_FN(sie_tag_spigot_new, sie_Tag_Spigot,
                          self, tag, (sie_Tag *tag),
                          sie_tag_spigot_init(self, tag));

sie_Output *sie_tag_spigot_get_inner(sie_Tag_Spigot *self)
{
    if (self->has_been_output)
        return NULL;

    self->output = sie_output_maybe_reuse(self->output);
    sie_output_grow_to(self->output, 0, 1);
    sie_output_set_raw(self->output, 0, 0,
                       sie_string_value(self->tag->value),
                       sie_string_length(self->tag->value));
    self->output->v_guts[0].size = 1;
    self->output->num_scans = 1;
    self->has_been_output = 1;

    return self->output;
}

void sie_tag_spigot_clear_output(sie_Tag_Spigot *self)
{
    self->output = sie_output_maybe_reuse(self->output);
    sie_output_clear_and_shrink(self->output);
}

size_t sie_tag_spigot_seek(sie_Tag_Spigot *self, size_t target)
{
    self->has_been_output = (target == 0) ? 0 : 1;
    return self->has_been_output;
}

size_t sie_tag_spigot_tell(sie_Tag_Spigot *self)
{
    return self->has_been_output ? 1 : 0;
}

int sie_tag_spigot_done(sie_Tag_Spigot *self)
{
    return self->has_been_output ? 1 : 0;
}

SIE_CLASS(sie_Tag_Spigot, sie_Spigot,
          SIE_MDEF(sie_destroy, sie_tag_spigot_destroy)
          SIE_MDEF(sie_spigot_get_inner, sie_tag_spigot_get_inner)
          SIE_MDEF(sie_spigot_clear_output, sie_tag_spigot_clear_output)
          SIE_MDEF(sie_spigot_seek, sie_tag_spigot_seek)
          SIE_MDEF(sie_spigot_tell, sie_tag_spigot_tell)
          SIE_MDEF(sie_spigot_done, sie_tag_spigot_done));
