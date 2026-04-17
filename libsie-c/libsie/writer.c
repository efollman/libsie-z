/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#include "sie_config.h"

#define FLUSH_SIZE 65536

static const char * const header =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<sie version=\"1.0\" xmlns=\"http://www.somat.com/SIE\">\n"
    "<!-- SIE format standard definitions: -->\n"
    " <!-- SIE stream decoder: -->\n"
    " <decoder id=\"0\">\n"
    "  <loop>\n"
    "   <read var=\"size\" bits=\"32\" type=\"uint\" endian=\"big\"/>\n"
    "   <read var=\"group\" bits=\"32\" type=\"uint\" endian=\"big\"/>\n"
    "   <read var=\"syncword\" bits=\"32\" type=\"uint\"\n"
    "         endian=\"big\" value=\"0x51EDA7A0\"/>\n"
    "   <read var=\"payload\" octets=\"{$size - 20}\" type=\"raw\"/>\n"
    "   <read var=\"checksum\" bits=\"32\" type=\"uint\" endian=\"big\"/>\n"
    "   <read var=\"size2\" bits=\"32\" type=\"uint\"\n"
    "         endian=\"big\" value=\"{$size}\"/>\n"
    "  </loop>\n"
    " </decoder>\n"
    " <tag id=\"sie:xml_metadata\" group=\"0\" format=\"text/xml\"/>\n"
    "\n"
    " <!-- SIE index block decoder:  v0=offset, v1=group -->\n"
    " <decoder id=\"1\">\n"
    "  <loop>\n"
    "   <read var=\"v0\" bits=\"64\" type=\"uint\" endian=\"big\"/>\n"
    "   <read var=\"v1\" bits=\"32\" type=\"uint\" endian=\"big\"/>\n"
    "   <sample/>\n"
    "  </loop>\n"
    " </decoder>\n"
    " <tag id=\"sie:block_index\" group=\"1\" decoder=\"1\"/>\n"
    "\n"
    "<!-- Stream-specific definitions begin here: -->\n"
    "\n";

SIE_CONTEXT_OBJECT_API_NEW_FN(
    sie_writer_new, sie_Writer, self, ctx_obj,
    (void *ctx_obj, sie_Writer_Fn *writer_fn, void *user),
    sie_writer_init(self, ctx_obj, writer_fn, user));

void sie_writer_init(sie_Writer *self, void *ctx_obj,
                     sie_Writer_Fn *writer_fn, void *user)
{
    sie_context_object_init(SIE_CONTEXT_OBJECT(self), ctx_obj);
    self->writer_fn = writer_fn;
    self->user = user;
    self->do_index = 1;
    self->next_ids[SIE_WRITER_ID_GROUP] = 2;
    self->next_ids[SIE_WRITER_ID_DECODER] = 2;
}

int sie_writer_flush_xml(sie_Writer *self)
{
    if (sie_vec_size(self->xml_buf))
        sie_writer_write_block(self, SIE_XML_GROUP, self->xml_buf,
                               sie_vec_size(self->xml_buf));
    sie_vec_clear(self->xml_buf);
    return 1; /* KLUDGE */
}

static void flush_index(sie_Writer *self)
{
    if (sie_vec_size(self->index_buf))
        sie_writer_write_block(self, SIE_INDEX_GROUP, self->index_buf,
                               sie_vec_size(self->index_buf));
    sie_vec_clear(self->index_buf);
}

static void maybe_flush_index(sie_Writer *self)
{
    if (sie_vec_size(self->index_buf) > FLUSH_SIZE)
        flush_index(self);
}

static void maybe_flush_xml(sie_Writer *self)
{
    if (sie_vec_size(self->xml_buf) > FLUSH_SIZE)
        sie_writer_flush_xml(self);
}

void sie_writer_destroy(sie_Writer *self)
{
    sie_writer_flush_xml(self);
    flush_index(self);
    sie_vec_free(self->xml_buf);
    sie_vec_free(self->index_buf);
    sie_context_object_destroy(SIE_CONTEXT_OBJECT(self));
}

int sie_writer_xml_string(sie_Writer *self,
                          const char *data, size_t size)
{
    sie_vec_memcat(self, &self->xml_buf, data, size);
    maybe_flush_xml(self);
    return 1; /* KLUDGE */
}

int sie_writer_xml_node(sie_Writer *self, sie_XML *node)
{
    sie_xml_output(node, &self->xml_buf, 1);
    maybe_flush_xml(self);
    return 1; /* KLUDGE */
}

int sie_writer_xml_header(sie_Writer *self)
{
    sie_writer_xml_string(self, header, strlen(header));
    return 1; /* KLUDGE */
}

int sie_writer_write_block(sie_Writer *self, sie_id group,
                           const char *data, size_t size)
{
    sie_uint32 block_size = (sie_uint32)size + SIE_OVERHEAD_SIZE;
    sie_uint32 group_be = sie_hton32(group);
    sie_assert(size + SIE_OVERHEAD_SIZE == block_size, self);

    if (self->writer_fn) {
        sie_uint32 block_size_be = sie_hton32(block_size);
        sie_uint32 magic_be = sie_hton32(SIE_MAGIC);
        sie_uint32 crc_be;
        char *buf = sie_malloc(self, block_size);
        size_t out;

        sie_set_uint32(buf + 0, block_size_be);
        sie_set_uint32(buf + 4, group_be);
        sie_set_uint32(buf + 8, magic_be);
        memcpy(buf + 12, data, size);
        crc_be = sie_hton32(sie_crc((unsigned char *)buf, size + 12));
        sie_set_uint32(buf + 12 + size, crc_be);
        sie_set_uint32(buf + 16 + size, block_size_be);

        out = self->writer_fn(self->user, buf, block_size);
        free(buf);
        if (out != block_size)
            return 0;
    }

    if (self->do_index && group != SIE_INDEX_GROUP) {
        size_t sz = sie_vec_size(self->index_buf);
        sie_vec_set_size(self->index_buf, sz + 12);
        sie_set_uint64(self->index_buf + sz, sie_hton64(self->offset));
        sie_set_uint32(self->index_buf + sz + 8, group_be);
        maybe_flush_index(self);
    }
    self->offset += block_size;

    return 1; /* KLUDGE */
}

sie_id sie_writer_next_id(sie_Writer *self, int type)
{
    return self->next_ids[type]++;
}

static sie_id next_group(sie_File *file)
{
    sie_XML *top = SIE_INTAKE(file)->xml->sie_node;
    sie_XML *cur = top;
    sie_String *group_s = sie_literal(file, group);
    sie_id highest = file->highest_group;

    do {
        if (cur->type == SIE_XML_ELEMENT) {
            sie_String *val_s = sie_xml_get_attribute_s(cur, group_s);
            if (val_s) {
                const char *val_c = sie_string_value(val_s);
                sie_id val = sie_strtoid(file, val_c);
                if (highest < val)
                    highest = val;
            }
        }
    } while ((cur = sie_xml_walk_next(cur, top, SIE_XML_DESCEND)));

    return highest + 1;
}

void sie_writer_prepare_append(sie_Writer *self, void *intake)
{
    sie_Class *intake_class = sie_class_of(intake);
    if (intake_class == SIE_CLASS_FOR_TYPE(sie_File)) {
        char *unindexed = NULL;
        size_t unindexed_size;
        sie_file_get_unindexed_blocks(intake, &unindexed, &unindexed_size);
        sie_vec_append(self->index_buf, unindexed, unindexed_size);
        maybe_flush_index(self);
        free(unindexed);
        self->offset = SIE_FILE(intake)->last_offset;
    } else {
        self->do_index = 0;
    }
    self->next_ids[SIE_WRITER_ID_GROUP] = next_group(intake);
    self->next_ids[SIE_WRITER_ID_TEST] =
        sie_id_map_get_max_id(SIE_INTAKE(intake)->xml->test_map) + 1;
    self->next_ids[SIE_WRITER_ID_CH] =
        sie_id_map_get_max_id(SIE_INTAKE(intake)->xml->channel_map) + 1;
    self->next_ids[SIE_WRITER_ID_DECODER] =
        sie_id_map_get_max_id(SIE_INTAKE(intake)->xml->decoder_map) + 1;
}

sie_uint64 sie_writer_total_size(sie_Writer *self,
                                 sie_uint64 addl_bytes,
                                 sie_uint64 addl_blocks)
{
    sie_uint64 total_size = self->offset;
    size_t index_size = sie_vec_size(self->index_buf);

    if (sie_vec_size(self->xml_buf)) {
        total_size += sie_vec_size(self->xml_buf) + SIE_OVERHEAD_SIZE;
        if (self->do_index) {
            index_size += 12;
            if (index_size > FLUSH_SIZE) {
                total_size += index_size + SIE_OVERHEAD_SIZE;
                index_size = 0;
            }
        }
    }

    total_size += addl_bytes + addl_blocks * SIE_OVERHEAD_SIZE;

    if (self->do_index) {
        /* KLUDGE this could probably be a lot more clever */
        while (addl_blocks--) {
            index_size += 12;
            if (index_size > FLUSH_SIZE) {
                total_size += index_size + SIE_OVERHEAD_SIZE;
                index_size = 0;
            }
        }

        if (index_size)
            total_size += index_size + SIE_OVERHEAD_SIZE;
    }

    return total_size;
}

SIE_CLASS(sie_Writer, sie_Context_Object,
          SIE_MDEF(sie_destroy, sie_writer_destroy));
