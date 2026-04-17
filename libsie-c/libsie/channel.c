/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#include "sie_config.h"

#include "sie_channel.h"

void sie_channel_init(sie_Channel *self, void *intake, sie_id id)
{
    const char *id_s;

    sie_ref_init(SIE_REF(self), intake);
    self->intake = sie_retain(intake);
    self->id = id;
    sie_assert(self->id != SIE_NULL_ID, self);
    self->raw_xml =
        sie_retain(sie_id_map_get(self->intake->xml->channel_map, self->id));
    sie_assert(self->raw_xml, self);
    self->toplevel_group = SIE_NULL_ID;

    id_s = sie_string_value(sie_xml_get_attribute_literal(self->raw_xml, id));
    sie_assert(id_s && sie_strtoid(self, id_s) == self->id, self);

    self->name = sie_xml_get_attribute_literal(self->raw_xml, name);

    if (!sie_xml_get_attribute_literal(self->raw_xml, base))
        self->expanded_xml = sie_retain(self->raw_xml);
}

static void expand_xml(sie_Channel *self)
{
    if (!self->expanded_xml) {
        self->expanded_xml = sie_xml_expand(self->intake->xml,
                                            sie_literal(self, ch), self->id);
        if (!self->name)
            self->name = sie_xml_get_attribute_literal(self->expanded_xml,
                                                       name);
    }
}

static void expand_dimensions(sie_Channel *self)
{
    if (!self->dimensions) {
        sie_XML *cur;
        const char *group_s;
        size_t num_dimensions = 0;

        expand_xml(self);
        group_s =
            sie_string_value(sie_xml_get_attribute_literal(self->expanded_xml,
                                                           group));
        self->toplevel_group =
            group_s ? sie_strtoid(self, group_s) : SIE_NULL_ID;
        
        cur = self->expanded_xml->child;
        while (cur) {
            if ((cur->type == SIE_XML_ELEMENT &&
                 cur->value.element.name == sie_literal(self, dim))) {
                sie_Dimension *dim = sie_dimension_new(self->intake, cur,
                                                       self->toplevel_group);
                sie_vec_reserve(self->dimensions, dim->index + 1);
                sie_release(self->dimensions[dim->index]);
                self->dimensions[dim->index] = dim;
                if (sie_vec_raw_size(self->dimensions) <= dim->index)
                    sie_vec_raw_size(self->dimensions) = dim->index + 1;
                num_dimensions++;
            }
            cur = sie_xml_walk_next(cur, self->expanded_xml,
                                    SIE_XML_NO_DESCEND);
        }
        sie_assert(num_dimensions == sie_vec_size(self->dimensions), self);
    }
}

void sie_channel_destroy(sie_Channel *self)
{
    sie_Dimension **dim;
    sie_vec_forall(self->dimensions, dim) {
        sie_release(*dim);
    }
    sie_vec_free(self->dimensions);
    sie_release(self->expanded_xml);
    sie_release(self->raw_xml);
    sie_release(self->intake);
    sie_ref_destroy(SIE_REF(self));
}

sie_Spigot *sie_channel_attach_spigot(sie_Channel *self)
{
    expand_dimensions(self);
    return SIE_SPIGOT(sie_channel_spigot_new(self));
}

void sie_channel_dump(sie_Channel *self, FILE *stream)
{
    sie_Tag *tag;
    sie_Dimension **dim_p;
    sie_Iterator *iter;
    expand_dimensions(self);
    fprintf(stream, "Channel id %d, \"%s\":\n",
            sie_get_id(self), sie_get_name(self));
    if (self->toplevel_group != SIE_NULL_ID)
        fprintf(stream, "  group %d\n", self->toplevel_group);
    iter = sie_get_tags(self);
    while ((tag = sie_iterator_next(iter)))
        sie_tag_dump(tag, stream, "  ");
    sie_release(iter);
    sie_vec_forall(self->dimensions, dim_p) {
        sie_dimension_dump(*dim_p, stream);
    }
}

sie_Iterator *sie_channel_get_tags(sie_Channel *self)
{
    expand_xml(self);
    return SIE_ITERATOR(sie_xml_iterator_new(
                            self->expanded_xml,
                            sie_literal(self, tag), NULL, NULL,
                            sie_tag_xml_iterator_realize,
                            self->intake));
}

sie_Iterator *sie_channel_get_dimensions(sie_Channel *self)
{
    expand_dimensions(self);
    return SIE_ITERATOR(sie_vec_iterator_new(self, self, self->dimensions));
}

sie_Dimension *sie_channel_get_dimension(sie_Channel *self, sie_id index)
{
    expand_dimensions(self);
    if (index >= sie_vec_size(self->dimensions))
        return NULL;
    return sie_retain(self->dimensions[index]);
}

sie_String *sie_channel_get_name_s(sie_Channel *self)
{
    if (!self->name)
        expand_xml(self);
    return self->name;
}

sie_id sie_channel_get_id(sie_Channel *self)
{
    return self->id;
}

sie_Test *sie_channel_get_containing_test(sie_Channel *self)
{
    /* still a bit of a KLUDGE */
    sie_XML *raw_test_node = self->raw_xml->parent;

    if (raw_test_node && raw_test_node->type == SIE_XML_ELEMENT &&
        raw_test_node->value.element.name == sie_literal(self, test)) {
        sie_id test_id =
            sie_strtoid(self, sie_xml_get_attribute(raw_test_node, "id"));
        return sie_get_test(self->intake, test_id);
    } else {
        return NULL;
    }
}

SIE_CONTEXT_OBJECT_NEW_FN(sie_channel_new, sie_Channel, self, intake,
                          (void *intake, sie_id id),
                          sie_channel_init(self, intake, id));

SIE_CLASS(sie_Channel, sie_Ref,
          SIE_MDEF(sie_destroy, sie_channel_destroy)
          SIE_MDEF(sie_get_tags, sie_channel_get_tags)
          SIE_MDEF(sie_get_dimensions, sie_channel_get_dimensions)
          SIE_MDEF(sie_get_dimension, sie_channel_get_dimension)
          SIE_MDEF(sie_get_name_s, sie_channel_get_name_s)
          SIE_MDEF(sie_get_id, sie_channel_get_id)
          SIE_MDEF(sie_get_containing_test, sie_channel_get_containing_test)
          SIE_MDEF(sie_attach_spigot, sie_channel_attach_spigot));


void sie_channel_spigot_init(sie_Channel_Spigot *self, sie_Channel *channel)
{
    size_t mark;
    sie_Dimension **dimension_p;
    size_t num_dims;
    sie_Decoder *decoder;

    sie_spigot_init(SIE_SPIGOT(self), channel);

    mark = sie_cleanup_mark(self);
    sie_recursion_limit(self);

    self->group = channel->toplevel_group;
    num_dims = sie_vec_size(channel->dimensions);
    sie_assert(num_dims, self);
    self->combiner = sie_combiner_new(self, num_dims);
    self->transform = sie_transform_new(sie_context(self), num_dims);
    if (self->group == SIE_NULL_ID)
        self->group = channel->dimensions[0]->group;
    self->decoder_id = channel->dimensions[0]->decoder_id;
    sie_vec_forall(channel->dimensions, dimension_p) {
        sie_Dimension *dimension = *dimension_p;
        sie_assert(dimension->group == self->group, self);
        sie_assert(dimension->decoder_id == self->decoder_id, self);
        sie_combiner_add_mapping(self->combiner,
                                 dimension->decoder_v,
                                 dimension->index);
        if (dimension->xform_node)
            sie_transform_set_from_xform_node(self->transform,
                                              dimension->index,
                                              dimension->xform_node,
                                              channel->intake);
    }
    sie_assertf(self->group != SIE_NULL_ID,
                (self, "Tried to attach a spigot to an abstract channel."));
    sie_assert(self->decoder_id != SIE_NULL_ID, self);

    self->group_ref = sie_group_new(channel->intake, self->group);
    self->group_spigot = sie_attach_spigot(self->group_ref);

    decoder = sie_id_map_get(channel->intake->xml->compiled_decoder_map,
                             self->decoder_id);
    sie_assert(decoder, self);
    self->machine = sie_decoder_machine_new(decoder);

    sie_cleanup_pop_mark(self, mark);
}

void sie_channel_spigot_destroy(sie_Channel_Spigot *self)
{
    sie_release(self->machine);
    sie_release(self->group_spigot);
    sie_release(self->group_ref);
    if (self->transform) sie_transform_free(self->transform);
    sie_release(self->combiner);
    sie_spigot_destroy(SIE_SPIGOT(self));
}

SIE_CONTEXT_OBJECT_NEW_FN(sie_channel_spigot_new, sie_Channel_Spigot,
                          self, channel, (sie_Channel *channel),
                          sie_channel_spigot_init(self, channel));

sie_Output *sie_channel_spigot_get_inner(sie_Channel_Spigot *self)
{
    sie_Output *group_output;
    sie_Output *combiner_output = NULL;

    group_output = sie_spigot_get(self->group_spigot);
    if (!group_output)
        return NULL;

    sie_decoder_machine_prep(self->machine,
                             group_output->v[0].raw[0].ptr,
                             group_output->v[0].raw[0].size);
    sie_decoder_machine_run(self->machine);
    combiner_output =
        sie_combiner_combine(self->combiner, self->machine->output);
    if (!self->transforms_disabled)
        sie_transform_apply(self->transform, combiner_output);
    combiner_output->block = group_output->block;

    return combiner_output;
}

void sie_channel_spigot_clear_output(sie_Channel_Spigot *self)
{
    sie_spigot_clear_output(self->group_spigot);
    self->machine->output = sie_output_maybe_reuse(self->machine->output);
    sie_output_clear_and_shrink(self->machine->output);
    self->combiner->output = sie_output_maybe_reuse(self->combiner->output);
    sie_output_clear_and_shrink(self->combiner->output);
}

size_t sie_channel_spigot_seek(sie_Channel_Spigot *self, size_t target)
{
    return sie_spigot_seek(self->group_spigot, target);
}

size_t sie_channel_spigot_tell(sie_Channel_Spigot *self)
{
    return sie_spigot_tell(self->group_spigot);
}

int sie_channel_spigot_spigot_done(sie_Channel_Spigot *self)
{
    return sie_spigot_done(self->group_spigot);
}

void sie_channel_spigot_disable_transforms(sie_Channel_Spigot *self,
                                           int disable)
{
    self->transforms_disabled = disable;
}

void sie_channel_spigot_transform_output(sie_Channel_Spigot *self,
                                         sie_Output *output)
{
    sie_transform_apply(self->transform, output);
}

SIE_CLASS(sie_Channel_Spigot, sie_Spigot,
          SIE_MDEF(sie_destroy, sie_channel_spigot_destroy)
          SIE_MDEF(sie_spigot_get_inner, sie_channel_spigot_get_inner)
          SIE_MDEF(sie_spigot_clear_output, sie_channel_spigot_clear_output)
          SIE_MDEF(sie_spigot_seek, sie_channel_spigot_seek)
          SIE_MDEF(sie_spigot_tell, sie_channel_spigot_tell)
          SIE_MDEF(sie_spigot_done, sie_channel_spigot_spigot_done)
          SIE_MDEF(sie_spigot_disable_transforms,
                   sie_channel_spigot_disable_transforms)
          SIE_MDEF(sie_spigot_transform_output,
                   sie_channel_spigot_transform_output));
