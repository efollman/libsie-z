/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#include "sie_config.h"

void sie_intake_init(sie_Intake *self, void *ctx_obj)
{
    sie_ref_init(SIE_REF(self), ctx_obj);
    self->xml = sie_xml_definition_new(self);
}

void sie_intake_destroy(sie_Intake *self)
{
    sie_release(self->xml);
    sie_ref_destroy(SIE_REF(self));
}

sie_Test *sie_intake_get_test(sie_Intake *self, sie_id id)
{
    sie_XML *test_xml = sie_xml_expand(self->xml, sie_literal(self, test), id);
    sie_Test *ref;
    sie_assert(test_xml, self);
    ref = sie_test_new(self, test_xml);
    sie_release(test_xml);
    return ref;
}

sie_Channel *sie_intake_get_channel(sie_Intake *self, sie_id id)
{
    return sie_channel_new(self, id);
}

static void *realize_intake_channel(void *ctx_obj, void *v_xml, void *v_self)
{
    sie_id id = sie_strtoid(v_self, sie_xml_get_attribute(v_xml, "id"));
    return sie_intake_get_channel(v_self, id);
}

static void *realize_intake_public_channel(void *ctx_obj,
                                           void *v_xml, void *v_self)
{
    sie_Intake *self = v_self;
    sie_id id = sie_strtoid(self, sie_xml_get_attribute(v_xml, "id"));

    if (self->xml->any_private_attrs) {
        const char *private = sie_xml_get_attribute(v_xml, "private");

        if (!private || !strcmp(private, "") || !strcmp(private, "0"))
            return sie_intake_get_channel(v_self, id);
        else
            return NULL;
    } else {
        void *volatile retval = NULL;
        sie_Channel *channel = sie_intake_get_channel(v_self, id);

        SIE_TRY(ctx_obj) {
            const char *name;
            /* KLUDGE check internal channels */
            sie_release(sie_attach_spigot(channel));
            name = sie_get_name(channel);
            if (name && strlen(name) && !strchr(name, '#'))
                retval = channel;
        } SIE_CATCH(ex) {
            (void)ex;
        } SIE_NO_FINALLY();

        if (!retval)
            sie_release(channel);

        return retval;
    }
}

sie_Iterator *sie_intake_get_all_channels(sie_Intake *self)
{
    return SIE_ITERATOR(sie_id_map_iterator_new(self->xml->channel_map,
                                                realize_intake_channel,
                                                self));
}

sie_Iterator *sie_intake_get_channels(sie_Intake *self)
{
    return SIE_ITERATOR(sie_id_map_iterator_new(self->xml->channel_map,
                                                realize_intake_public_channel,
                                                self));
}

static void *realize_intake_test(void *ctx_obj, void *v_xml, void *v_self)
{
    sie_id id = sie_strtoid(v_self, sie_xml_get_attribute(v_xml, "id"));
    return sie_intake_get_test(v_self, id);
}

static void *realize_intake_public_test(void *ctx_obj, void *v_xml, void *v_self)
{
    sie_id id = sie_strtoid(v_self, sie_xml_get_attribute(v_xml, "id"));
    const char *private = sie_xml_get_attribute(v_xml, "private");
    if (!private || !strcmp(private, "") || !strcmp(private, "0"))
        return sie_intake_get_test(v_self, id);
    else
        return NULL;
}

sie_Iterator *sie_intake_get_tests(sie_Intake *self)
{
    return SIE_ITERATOR(sie_id_map_iterator_new(self->xml->test_map,
                                                realize_intake_test,
                                                self));
}

sie_Iterator *sie_intake_get_all_tests(sie_Intake *self)
{
    return SIE_ITERATOR(sie_id_map_iterator_new(self->xml->test_map,
                                                realize_intake_public_test,
                                                self));
}

sie_Iterator *sie_intake_get_tags(sie_Intake *self)
{
    return SIE_ITERATOR(sie_xml_iterator_new(
                            self->xml->sie_node,
                            sie_literal(self, tag), NULL, NULL,
                            sie_tag_xml_iterator_realize,
                            self));
}

SIE_METHOD(sie_get_group_handle, void *, self,
           (void *self, sie_id group), (self, group));
SIE_METHOD(sie_get_group_num_blocks, size_t, self,
           (void *self, void *group_handle), (self, group_handle));
SIE_METHOD(sie_get_group_num_bytes, sie_uint64, self,
           (void *self, void *group_handle), (self, group_handle));
SIE_METHOD(sie_get_group_block_size, sie_uint32, self,
           (void *self, void *group_handle, size_t entry),
           (self, group_handle, entry));
SIE_VOID_METHOD(sie_read_group_block, self,
                (void *self, void *group_handle,
                 size_t index, sie_Block *block),
                (self, group_handle, index, block));
SIE_METHOD(sie_is_group_closed, int, self,
           (void *self, void *group_handle), (self, group_handle));

SIE_API_METHOD(sie_add_stream_data, size_t, 0, self,
               (void *self, const void *data, size_t size),
               (self, data, size));

SIE_CLASS(sie_Intake, sie_Ref,
          SIE_MDEF(sie_destroy, sie_intake_destroy)
          SIE_MDEF(sie_get_channel, sie_intake_get_channel)
          SIE_MDEF(sie_get_test, sie_intake_get_test)
          SIE_MDEF(sie_get_channels, sie_intake_get_channels)
          SIE_MDEF(sie_get_all_channels, sie_intake_get_all_channels)
          SIE_MDEF(sie_get_tests, sie_intake_get_tests)
          SIE_MDEF(sie_get_all_tests, sie_intake_get_all_tests)
          SIE_MDEF(sie_get_tags, sie_intake_get_tags)
          SIE_MDEF(sie_get_group_handle, sie_abstract_method)
          SIE_MDEF(sie_get_group_num_blocks, sie_abstract_method)
          SIE_MDEF(sie_get_group_num_bytes, sie_abstract_method)
          SIE_MDEF(sie_get_group_block_size, sie_abstract_method)
          SIE_MDEF(sie_read_group_block, sie_abstract_method)
          SIE_MDEF(sie_is_group_closed, sie_abstract_method)
          SIE_MDEF(sie_add_stream_data, sie_abstract_method));
