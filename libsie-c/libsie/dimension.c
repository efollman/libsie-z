/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#include "sie_config.h"

#include "sie_dimension.h"

void sie_dimension_init(sie_Dimension *self, void *intake,
                        sie_XML *node, sie_id toplevel_group)
{
    sie_String *index_s;
    sie_String *group_s;
    sie_XML *cur;

    sie_ref_init(SIE_REF(self), intake);

    self->decoder_id = SIE_NULL_ID;

    self->intake = sie_retain(intake);
    self->xml = sie_retain(node);

    index_s = sie_xml_get_attribute_literal(node, index);
    group_s = sie_xml_get_attribute_literal(node, group);

    sie_assert(index_s, self);
    self->group =
        group_s ? sie_strtoid(self, sie_sv(group_s)) : toplevel_group;
    self->index = sie_strtoid(self, sie_sv(index_s));
    
    cur = node->child;
    while (cur) {
        if (cur->type == SIE_XML_ELEMENT) {
            if (cur->value.element.name == sie_literal(self, data)) {
                sie_String *decoder_s =
                    sie_xml_get_attribute_literal(cur, decoder);
                sie_String *v_s = sie_xml_get_attribute_literal(cur, v);
                sie_assert(decoder_s && v_s, self);
                self->decoder_id = sie_strtoid(self, sie_sv(decoder_s));
                self->decoder_v = sie_strtosizet(self, sie_sv(v_s));
            } else if (cur->value.element.name == sie_literal(self, xform)) {
                sie_assert(!self->xform_node, self);
                self->xform_node = sie_retain(cur);
            }
        }
        cur = sie_xml_walk_next(cur, node, SIE_XML_NO_DESCEND);
    }

}

void sie_dimension_destroy(sie_Dimension *self)
{
    sie_release(self->xform_node);
    sie_release(self->xml);
    sie_release(self->intake);
    sie_ref_destroy(SIE_REF(self));
}

SIE_CONTEXT_OBJECT_NEW_FN(sie_dimension_new, sie_Dimension, self, intake,
                          (void *intake, sie_XML *node,
                           sie_id toplevel_group),
                          sie_dimension_init(self, intake, node,
                                             toplevel_group));

void sie_dimension_dump(sie_Dimension *self, FILE *stream)
{
    sie_Tag *tag;
    sie_Iterator *iter;
    fprintf(stream, "  Dimension index %d:\n", self->index);
    fprintf(stream, "    group %d\n", self->group);
    fprintf(stream, "    decoder_id %d\n", self->decoder_id);
    fprintf(stream, "    decoder_v %"APR_SIZE_T_FMT"\n", self->decoder_v);
    iter = sie_get_tags(self);
    while ((tag = sie_iterator_next(iter)))
        sie_tag_dump(tag, stream, "    ");
    sie_release(iter);
}

sie_Iterator *sie_dimension_get_tags(sie_Dimension *self)
{
    return SIE_ITERATOR(sie_xml_iterator_new(
                            self->xml,
                            sie_literal(self, tag), NULL, NULL,
                            sie_tag_xml_iterator_realize,
                            self->intake));
}

sie_id sie_dimension_get_index(sie_Dimension *self)
{
    return self->index;
}

SIE_CLASS(sie_Dimension, sie_Ref,
          SIE_MDEF(sie_get_tags, sie_dimension_get_tags)
          SIE_MDEF(sie_get_index, sie_dimension_get_index)
          SIE_MDEF(sie_destroy, sie_dimension_destroy));
