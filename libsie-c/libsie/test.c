/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#include "sie_config.h"

#include "sie_test.h"

void sie_test_init(sie_Test *self, void *intake, sie_XML *node)
{
    const char *id_s = sie_xml_get_attribute(node, "id");

    sie_ref_init(SIE_REF(self), intake);
    self->intake = sie_retain(intake);
    self->node = sie_retain(node);

    sie_assert(id_s, self);
    self->id = sie_strtoid(self, id_s);
}

void sie_test_destroy(sie_Test *self)
{
    sie_release(self->node);
    sie_release(self->intake);
    sie_ref_destroy(SIE_REF(self));
}

sie_Iterator *sie_test_get_tags(sie_Test *self)
{
    return SIE_ITERATOR(sie_xml_iterator_new(
                            self->node, sie_literal(self, tag), NULL, NULL,
                            sie_tag_xml_iterator_realize, self->intake));
}

static void *realize_test_channel(void *ctx_obj, void *v_xml, void *v_test)
{
    sie_Test *test = v_test;
    sie_id id = sie_strtoid(test, sie_xml_get_attribute(v_xml, "id"));
    return sie_get_channel(test->intake, id);
}

static void *realize_test_public_channel(void *ctx_obj,
                                         void *v_xml, void *v_test)
{
    sie_Test *test = v_test;
    sie_id id = sie_strtoid(test, sie_xml_get_attribute(v_xml, "id"));
    const char *private = sie_xml_get_attribute(v_xml, "private");
    if (!private || !strcmp(private, "") || !strcmp(private, "0"))
        return sie_get_channel(test->intake, id);
    else
        return NULL;
}

sie_Iterator *sie_test_get_all_channels(sie_Test *self)
{
    return SIE_ITERATOR(sie_xml_iterator_new(
                            self->node, sie_literal(self, ch), NULL, NULL,
                            realize_test_channel, self));
}

sie_Iterator *sie_test_get_channels(sie_Test *self)
{
    return SIE_ITERATOR(sie_xml_iterator_new(
                            self->node, sie_literal(self, ch), NULL, NULL,
                            realize_test_public_channel, self));
}

sie_id sie_test_get_id(sie_Test *self)
{
    return self->id;
}

void sie_test_dump(sie_Test *self, FILE *stream)
{
    sie_Iterator *iter;
    sie_Tag *tag;
    sie_Channel *channel;
    fprintf(stream, "Test id %d:\n", sie_get_id(self));
    iter = sie_get_tags(self);
    while ((tag = sie_iterator_next(iter))) {
        sie_tag_dump(tag, stream, "  ");
    }
    sie_release(iter);
    iter = sie_get_channels(self);
    while ((channel = sie_iterator_next(iter))) {
        fprintf(stream, "  Channel id %d, \"%s\"\n",
                sie_get_id(channel), sie_get_name(channel));
    }
    sie_release(iter);
}

SIE_CONTEXT_OBJECT_NEW_FN(sie_test_new, sie_Test, self, intake,
                          (void *intake, sie_XML *node),
                          sie_test_init(self, intake, node));

SIE_CLASS(sie_Test, sie_Ref,
          SIE_MDEF(sie_destroy, sie_test_destroy)
          SIE_MDEF(sie_get_tags, sie_test_get_tags)
          SIE_MDEF(sie_get_channels, sie_test_get_channels)
          SIE_MDEF(sie_get_all_channels, sie_test_get_all_channels)
          SIE_MDEF(sie_get_id, sie_test_get_id));
