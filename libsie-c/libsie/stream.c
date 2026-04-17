/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#include "sie_config.h"

SIE_CONTEXT_OBJECT_API_NEW_FN(sie_stream_new, sie_Stream, self, ctx_obj,
                              (void *ctx_obj),
                              sie_stream_init(self, ctx_obj));

void sie_stream_init(sie_Stream *self, void *ctx_obj)
{
    sie_intake_init(SIE_INTAKE(self), ctx_obj);
    self->group_indexes = sie_id_map_new(self, 32);
    self->block = sie_block_new(self);
}

static void do_free_index(sie_id group, void *index_v, void *ignore)
{
    sie_Stream_Group_Index *index = index_v;
    sie_Stream_Group_Index_Entry *entry;
    sie_vec_forall(index->entries, entry) {
        free(entry->ptr);
    }
    sie_vec_free(index->entries);
    free(index);
}

void sie_stream_destroy(sie_Stream *self)
{
    sie_release(self->block);
    sie_id_map_foreach(self->group_indexes, do_free_index, NULL);
    sie_release(self->group_indexes);
    sie_intake_destroy(SIE_INTAKE(self));
}

static void index_add(sie_Stream *self, sie_id group,
                      void *ptr, sie_uint32 size)
{
    sie_Stream_Group_Index_Entry entry = { ptr, size };
    sie_Stream_Group_Index *index = sie_id_map_get(self->group_indexes, group);
    if (!index) {
        index = calloc(1, sizeof(*index));
        index->group = group;
        sie_id_map_set(self->group_indexes, group, index);
    }
    sie_debug((self, 10, "  index_add(group = %u, ptr = %p, size = %u)\n",
               group, ptr, size));
    if (size) {
        sie_assertf(!index->closed,
                    (self, "Received a block in a group already closed."));
        index->payload_size += size;
        sie_vec_push_back(index->entries, entry);
    } else {
        sie_debug((self, 10, "  closing group %u\n", group));
        index->closed = 1;
        free(ptr);
    }
}

static void add_block(void *self_v, sie_Block *block)
{
    sie_Stream *self = self_v;
    void *ptr;
    ptr = sie_malloc(self, block->size);
    sie_debug((self, 10, "  add_block: ptr = %p\n", ptr));
    memcpy(ptr, block->data, block->size);
    index_add(self, block->group, ptr,
              block->size - SIE_OVERHEAD_SIZE);
    if (block->group == 0)
        sie_xml_definition_add_string(self->parent.xml,
                                      block->data->payload,
                                      block->size - SIE_OVERHEAD_SIZE);
}

size_t sie_stream_add_data_guts(void *self, sie_Block *block, size_t *read,
                                sie_Add_Block_Fn *add_block_fn,
                                const void *data_v, size_t size)
{
    const char *data = data_v;
    size_t left = size;
    sie_debug((self, 10, "add_stream_data: size %"APR_SIZE_T_FMT" "
               "read %"APR_SIZE_T_FMT"\n", size, *read));

    while (left) {
        int read_header = *read < SIE_HEADER_SIZE;
        size_t to_copy = read_header ? SIE_HEADER_SIZE : block->size;
        to_copy -= *read;
        if (to_copy > left)
            to_copy = left;
        sie_debug((self, 10, "  copy %"APR_SIZE_T_FMT" bytes (%s)\n",
                   to_copy, read_header ? "header" : "body"));
        memcpy((char *)block->data + *read, data, to_copy);
        *read += to_copy;
        data += to_copy;
        left -= to_copy;
        if (*read == SIE_HEADER_SIZE) {
            block->size = sie_ntoh32(block->data->size);
            block->group = sie_ntoh32(block->data->group);
            sie_debug((self, 10, "  size %u group %u magic %08x\n",
                       block->size, block->group,
                       sie_ntoh32(block->data->magic)));
            if (block->size < SIE_OVERHEAD_SIZE)
                sie_errorf((self, "Block too small."));
            if (sie_ntoh32(block->data->magic) != SIE_MAGIC)
                sie_errorf((self, "Bad sync word."));
            sie_block_expand(block, block->size);
        } else if (*read == block->size) {
            char *data = (char *)block->data;
            sie_uint32 *trailer =
                (sie_uint32 *)(data + block->size - SIE_TRAILER_SIZE);
            sie_debug((self, 10, "  add_block: magic %08x group %d size %u\n",
                       sie_ntoh32(block->data->magic), block->group,
                       block->size));
            block->checksum = sie_ntoh32(trailer[0]);
            if (block->checksum) {
                sie_uint32 checksum = sie_crc((unsigned char *)block->data,
                                              block->size - SIE_TRAILER_SIZE);
                if (block->checksum != checksum)
                    sie_errorf((self, "Bad checksum, %08x != %08x.",
                                block->checksum, checksum));
            }
            add_block_fn(self, block);
            *read = 0;
        }
        sie_debug((self, 10, "  %"APR_SIZE_T_FMT" bytes left\n", left));
    }

    sie_debug((self, 10, "  %"APR_SIZE_T_FMT" bytes left over\n",
               *read));
    sie_debug((self, 10, "  add_stream_data returning %"APR_SIZE_T_FMT"\n",
               size));
    return size;
}

size_t sie_stream_add_stream_data(sie_Stream *self, const void *data_v,
                                  size_t size)
{
    return sie_stream_add_data_guts(self, self->block, &self->read,
                                    add_block, data_v, size);
}

void *sie_stream_get_group_handle(sie_Stream *self, sie_id group)
{
    return sie_id_map_get(self->group_indexes, group);
}

size_t sie_stream_get_group_num_blocks(sie_Stream *self, void *group_handle)
{
    sie_Stream_Group_Index *gi = group_handle;
    return gi->base + sie_vec_size(gi->entries);
}

sie_uint64 sie_stream_get_group_num_bytes(sie_Stream *self, void *group_handle)
{
    sie_Stream_Group_Index *gi = group_handle;
    return gi->payload_size;
}

sie_uint32 sie_stream_get_group_block_size(sie_Stream *self, void *group_handle,
                                           size_t entry)
{
    sie_Stream_Group_Index *gi = group_handle;
    if (entry < gi->base)
        sie_errorf((self, "Tried to get the size of an old stream block."));
    return gi->entries[entry - gi->base].size;
}

void sie_stream_read_group_block(sie_Stream *self, void *group_handle,
                                 size_t entry, sie_Block *block)
{
    sie_Stream_Group_Index *gi = group_handle;
    sie_Stream_Group_Index_Entry *ie;
    size_t i;
    if (entry < gi->base)
        sie_errorf((self, "Tried to read a block already read."));
    entry -= gi->base;
    ie = &gi->entries[entry];
    sie_debug((self, 10, "ptr = %p\n", ie->ptr));
    if (!ie->ptr)
        sie_errorf((self, "Tried to read a block already read."));
    sie_debug((self, 10, "sie_stream_read_group_block: ptr = %p\n", ie->ptr));
    sie_block_expand(block, ie->size + SIE_OVERHEAD_SIZE);
    memcpy(block->data, ie->ptr, ie->size + SIE_OVERHEAD_SIZE);
    free(ie->ptr);
    ie->ptr = NULL;
    block->group = gi->group;
    block->size = ie->size;
    block->max_size = ie->size + SIE_OVERHEAD_SIZE;
    block->checksum = 0;        /* KLUDGE */
    if (entry == gi->first) {
        for (i = entry + 1; i < sie_vec_size(gi->entries); i++) {
            if (gi->entries[i].ptr)
                break;
        }
        gi->first = i;
    }
    if (gi->first > sie_vec_size(gi->entries) / 2) {
        size_t new_size = sie_vec_size(gi->entries) - gi->first;
        memmove(gi->entries, gi->entries + gi->first,
                new_size * sizeof(*gi->entries));
        sie_vec_set_size(gi->entries, new_size);
        gi->base += gi->first;
        gi->first = 0;
    }
}

int sie_stream_is_group_closed(sie_Stream *self, void *group_handle)
{
    if (group_handle) {
        sie_Stream_Group_Index *gi = group_handle;
        return gi->closed;
    } else {
        return 0;
    }
}

SIE_CLASS(sie_Stream, sie_Intake,
          SIE_MDEF(sie_destroy, sie_stream_destroy)
          SIE_MDEF(sie_add_stream_data, sie_stream_add_stream_data)
          SIE_MDEF(sie_get_group_handle, sie_stream_get_group_handle)
          SIE_MDEF(sie_get_group_num_blocks, sie_stream_get_group_num_blocks)
          SIE_MDEF(sie_get_group_num_bytes, sie_stream_get_group_num_bytes)
          SIE_MDEF(sie_get_group_block_size, sie_stream_get_group_block_size)
          SIE_MDEF(sie_read_group_block, sie_stream_read_group_block)
          SIE_MDEF(sie_is_group_closed, sie_stream_is_group_closed));
