/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#define HASH_FUNCTION HASH_FNV

#include "sie_config.h"

#include <stdio.h>
#include <stdlib.h>

#include "sie_config.h"
#include "sie_vec.h"
#include "sie_debug.h"
#include "sie_byteswap.h"
#include "sie_file.h"
#include "sie_block.h"
#include "sie_spigot.h"
#include "sie_xml_merge.h"
#include "sie_sie_vec.h"
#include "sie_utils.h"
#include "sie_decoder.h"
#include "sie_group.h"

static sie_File_Group_Index *get_group_index(sie_File *self, sie_id group)
{
    sie_File_Group_Index *found;
    HASH_FIND(hh, self->group_indexes, &group, sizeof(group), found);
    return found;
}

static sie_File_Group_Index *intern_group_index(sie_File *self, sie_id group)
{
    sie_File_Group_Index *found = get_group_index(self, group);
    if (!found) {
        found = sie_calloc(self, sizeof(*found));
        found->group = group;
        HASH_ADD_KEYPTR(hh, self->group_indexes, &found->group,
                        sizeof(found->group), found);
        if (self->highest_group < group)
            self->highest_group = group;
    }
    return found;
}

static apr_off_t tell(apr_file_t *file)
{
    apr_off_t fpos = 0;
    apr_file_seek(file, APR_CUR, &fpos);
    return fpos;
}

typedef struct _scan_index_part {
    sie_uint64 offset;
    sie_uint32 size;
    sie_id group;
} scan_index_part;

#define SCAN_INDEX_PART_SIZE (1024 * 64)
#define SCAN_INDEX_PART_ENTRIES (SCAN_INDEX_PART_SIZE / sizeof(scan_index_part))

typedef struct _scan_index {
    scan_index_part **parts;
    size_t last_part_offset;
    int seen_index;
} scan_index;

static void scan_index_add(sie_File *self, scan_index *fi, sie_id group,
                           sie_uint64 offset, sie_uint32 size)
{
    sie_File_Group_Index *index = intern_group_index(self, group);
    scan_index_part *part;
    if (fi->last_part_offset == 0) {
        part = sie_calloc(self, SCAN_INDEX_PART_SIZE);
        sie_vec_push_back(fi->parts, part);
        fi->last_part_offset = SCAN_INDEX_PART_ENTRIES;
    }
    part = fi->parts[sie_vec_size(fi->parts) - 1];
    --fi->last_part_offset;
    if (!fi->seen_index) {
        if (!self->last_offset) {
            self->last_offset = offset + size;
        }
        if (group == SIE_INDEX_GROUP) {
            fi->seen_index = 0;
        } else {
            self->first_unindexed = offset;
            ++self->num_unindexed;
        }
    }
    part[fi->last_part_offset].offset = offset;
    part[fi->last_part_offset].size = size;
    part[fi->last_part_offset].group = group;
    ++index->scan_num_blocks;
}

static void scan_index_free(void *fi_v)
{
    scan_index *fi = fi_v;
    scan_index_part **curp;
    sie_vec_forall(fi->parts, curp) {
        free(*curp);
    }
    sie_vec_free(fi->parts);
    free(fi);
}

static apr_off_t add_index_block(sie_File *self, sie_Block *block,
                                 apr_off_t idx_offs, scan_index *fi)
{
    char *raw = block->data->payload;
    size_t entry;
    sie_uint64 initial_offset, offset, prev_offset;
    sie_uint32 group;
    sie_uint32 first_size, last_size;
    apr_off_t off = idx_offs - 4;
    apr_size_t size = 4;
    apr_off_t here = tell(self->fd);
    size_t num_payload_blocks;

    sie_debug((self, 4, "index found with contents:\n"));
    
    if (apr_file_seek(self->fd, APR_SET, &off) != APR_SUCCESS)
        sie_errorf((self, "Couldn't seek to size before index"));
    if (apr_file_read(self->fd, &last_size, &size) != APR_SUCCESS || size != 4)
        sie_errorf((self, "Couldn't read size before index"));
    last_size = sie_ntoh32(last_size);

    sie_assert(block->size >= 12, self);
    sie_assert((block->size % 12) == 0, self);

    initial_offset = sie_ntoh64(sie_get_uint64(raw));
    group = sie_ntoh32(sie_get_uint32(raw + 8));
    off = initial_offset;
    if (apr_file_seek(self->fd, APR_SET, &off) != APR_SUCCESS)
        sie_errorf((self, "Couldn't seek to size of first indexed block"));
    size = 4;
    if (apr_file_read(self->fd, &first_size, &size) != APR_SUCCESS || size != 4)
        sie_errorf((self, "Couldn't read size of first indexed block"));
    first_size = sie_ntoh32(first_size);

    num_payload_blocks = block->size / 12;
    prev_offset = idx_offs;
    for (entry = num_payload_blocks - 1; ; --entry) {
        apr_off_t bsize;
        offset = sie_ntoh64(sie_get_uint64(raw + (entry * 12)));
        group = sie_ntoh32(sie_get_uint32(raw + (entry * 12) + 8));
        bsize = prev_offset - offset;
        sie_debug((self, 4, "  offset %"APR_UINT64_T_FMT" group %u"
                   " size %"APR_OFF_T_FMT"\n",
                   offset, group, bsize));
        sie_assert(offset < prev_offset, self);
        if (prev_offset == idx_offs) {
            sie_assertf(last_size == bsize,
                        (self, "Last index entry computed size "
                         "does not agree with file"));
        } else if (offset == initial_offset) {
            sie_assertf(first_size == bsize,
                        (self, "First index entry computed size "
                         "does not agree with file"));
        }
        sie_assert(bsize >= 0 && bsize <= 0xffffffff, self);
        scan_index_add(self, fi, group, offset, (sie_uint32)bsize);
        prev_offset = offset;
        if (entry == 0) break;
    }

    if (apr_file_seek(self->fd, APR_SET, &here) != APR_SUCCESS)
        sie_errorf((self, "Couldn't seek back to initial position"));
    
    return initial_offset;
}

static void index_add(sie_File *self, sie_id group,
                      sie_uint64 offset, sie_uint32 size)
{
    sie_File_Group_Index_Entry entry = { offset, size };
    sie_File_Group_Index *index = get_group_index(self, group);
    sie_assert(index, self);
    if (size) {
        sie_assertf(!index->closed,
                    (self, "Received a block in a group already closed."));
        index->payload_size += size;
        if (!index->entries && index->scan_num_blocks)
            sie_vec_reserve(index->entries, index->scan_num_blocks);
        sie_vec_push_back(index->entries, entry);
    } else {
        sie_debug((self, 10, "  closing group %u\n", group));
        index->closed = 1;
    }
}

#undef SIE_VEC_CONTEXT_OBJECT
#define SIE_VEC_CONTEXT_OBJECT file

static void build_index(sie_File *file)
{
    size_t mark = sie_cleanup_mark(file);
    sie_Context *ctx = sie_context(file);
    sie_Block *block = sie_autorelease(sie_block_new(file->ctx));
    scan_index *fi;
    apr_off_t offset;
    apr_off_t file_size;
    int first = 1;
    size_t i;
    scan_index_part last_entry;
    scan_index_part **cur_partp;

    sie_error_context_auto(file, "Indexing file");
    fi = sie_calloc(file, sizeof(*fi));
    sie_cleanup_push(file, scan_index_free, fi);

    sie_progress_msg(file, "Indexing file");

    offset = 0;
    if (apr_file_seek(file->fd, APR_END, &offset) != APR_SUCCESS)
        sie_errorf((file, "Seek to end of file failed"));
    file_size = offset;
    sie_progress(file, 0, file_size);

    if (ctx->ignore_trailing_garbage) {
        offset = sie_file_find_block_backward(file, block,
                                              ctx->ignore_trailing_garbage);
    } else {
        offset = sie_file_read_block_backward(file, block);
    }

    if (offset < 0)
        sie_errorf((file, "No valid block %s end of file",
                    ctx->ignore_trailing_garbage ? "near" : "at"));

    do {
        sie_debug((file->ctx, 4, "offset %"APR_OFF_T_FMT" group %d "
                   "size %u csum %08x\n",
                   offset, block->group, block->size, block->checksum));
        scan_index_add(file, fi, block->group, offset,
                       block->size + SIE_OVERHEAD_SIZE);
        if (block->group == SIE_INDEX_GROUP) {
            offset = add_index_block(file, block, offset, fi);
            if (apr_file_seek(file->fd, APR_SET, &offset) != APR_SUCCESS)
                sie_errorf((file, "Failed to seek to initial index offset"));
        }
        if (offset >= 0)
            sie_progress(file, file_size - offset, file_size);
    } while (offset > 0 &&
             (offset = sie_file_read_block_backward(file, block)) >= 0);
    if (offset < 0)
        sie_errorf((file, "Error reading file."));

    sie_debug((file, 10, "index:\n"));
    i = fi->last_part_offset;
    while ( (cur_partp = sie_vec_back(fi->parts)) ) {
        scan_index_part *cur_part = *cur_partp;
        for ( ; i < SCAN_INDEX_PART_ENTRIES; ++i) {
            if (!first)
                sie_debug((file, 10, "loffs %"APR_UINT64_T_FMT" + lsize %d == ",
                           last_entry.offset, last_entry.size));
            sie_debug((file, 10, "offset %"APR_UINT64_T_FMT" (size %d group %d)\n",
                       cur_part[i].offset, cur_part[i].size, cur_part[i].group));
            if (!first)
                sie_assert(last_entry.offset + last_entry.size == cur_part[i].offset,
                           file);
            index_add(file, cur_part[i].group,
                      cur_part[i].offset,
                      cur_part[i].size - SIE_OVERHEAD_SIZE);
            last_entry = cur_part[i];
            first = 0;
        }
        free(cur_part);
        sie_vec_pop_back(fi->parts);
        i = 0;
    }

    sie_cleanup_pop_mark(file, mark);
}

void sie_file_group_foreach(sie_File *self, sie_File_Group_Foreach_Fn *fn,
                            void *extra)
{
    sie_File_Group_Index *cur;
    for (cur = self->group_indexes; cur; cur = cur->hh.next)
        fn((sie_id)cur->group, cur, extra);
}

static void parse_xml(sie_File *file)
{
    size_t mark = sie_cleanup_mark(file);
    sie_Group *group = sie_autorelease(sie_group_new(file, SIE_XML_GROUP));
    sie_Spigot *spigot = sie_autorelease(sie_attach_spigot(group));
    sie_uint64 num_xml_blocks;
    sie_Output *output;

    num_xml_blocks = sie_spigot_seek(spigot, SIE_SPIGOT_SEEK_END);
    sie_spigot_seek(spigot, 0);

    sie_error_context_auto(file, "Reading XML metadata");

    sie_progress_msg(file, "Reading XML metadata");
    sie_progress(file, 0, num_xml_blocks);

    while ( (output = sie_spigot_get(spigot)) ) {
        sie_xml_definition_add_string(file->parent.xml,
                                      output->v[0].raw[0].ptr,
                                      output->v[0].raw[0].size);
        sie_progress(file, sie_spigot_tell(spigot), num_xml_blocks);
    }

    /* A nicer-looking error for no XML. */
    sie_assertf(file->parent.xml->sie_node,
                (file, "No XML metadata.  Valid SIE file?"));

    /* Try to close the top-level sie node to ensure the XML ended at
     * a valid place -- this will trigger an error if not. */
    sie_xml_definition_add_string(file->parent.xml, "</sie>", 6);

    sie_cleanup_pop_mark(file, mark);
}

static void file_open(sie_Context *ctx, apr_pool_t **pool, const char *name,
                      apr_file_t **fd, int opts)
{
    apr_status_t status;
    char strerror_buf[256];

    status = apr_pool_create(pool, ctx->pool);
    sie_assertf(status == APR_SUCCESS,
                (ctx, "APR pool creation failed: %s",
                 apr_strerror(status, strerror_buf,
                              sizeof(strerror_buf))));

    status = apr_file_open(fd, name,
                           APR_READ | APR_BINARY | APR_BUFFERED | opts,
                           APR_OS_DEFAULT, *pool);
    sie_assertf(status == APR_SUCCESS,
                (ctx, "SIE file '%s' could not be opened: %s",
                 name, apr_strerror(status, strerror_buf,
                                    sizeof(strerror_buf))));
}

static void shared_init(sie_File *self, const char *name, int opts)
{
    self->ctx = sie_context(self);
    file_open(self->ctx, &self->pool, name, &self->fd, opts);
}

void sie_file_barebones_init(sie_File *self, void *ctx_obj,
                             const char *name)
{
    sie_intake_init(SIE_INTAKE(self), ctx_obj);
    shared_init(self, name, 0);
}

SIE_CONTEXT_OBJECT_NEW_FN(
    sie_file_barebones_new, sie_File, self, ctx_obj,
    (void *ctx_obj, const char *name),
    sie_file_barebones_init(self, ctx_obj, name));

int sie_file_is_sie(void *ctx_obj, const char *name)
{
    if (ctx_obj) {
        int volatile error = 0;
        SIE_API_TRY(ctx_obj) {
            size_t mark = sie_cleanup_mark(ctx_obj);
            sie_File *file =
                sie_autorelease(sie_file_barebones_new(ctx_obj, name));
            sie_Block *block = sie_autorelease(sie_block_new(ctx_obj));
            sie_assertf(sie_file_read_block(file, block) != -1,
                        (ctx_obj, "No valid block at the beginning of '%s', "
                         "probably not an SIE file.", name));
            sie_cleanup_pop_mark(ctx_obj, mark);
        } SIE_END_API_TRY(error);
        return !error;
    }
    return 0;
}

SIE_CONTEXT_OBJECT_API_NEW_FN(sie_file_open, sie_File, self, ctx_obj,
                              (void *ctx_obj, const char *name),
                              sie_file_init(self, ctx_obj, name));

void sie_file_init(sie_File *self, void *ctx_obj, const char *name)
{
    size_t mark = sie_cleanup_mark(ctx_obj);
    sie_intake_init(SIE_INTAKE(self), ctx_obj);
    sie_error_context_auto(self, "Opening SIE file '%s'", name);
    shared_init(self, name, 0);

    build_index(self);
    parse_xml(self);
    
    sie_cleanup_pop_mark(self, mark);
}

void sie_file_destroy(sie_File *self)
{
    sie_File_Group_Index *cur, *next;
    for (cur = self->group_indexes; cur; cur = next) {
        next = cur->hh.next;
        HASH_DELETE(hh, self->group_indexes, cur);
        sie_vec_free(cur->entries);
        free(cur);
    }
    if (self->fd) apr_file_close(self->fd);
    if (self->pool) apr_pool_destroy(self->pool);
    sie_intake_destroy(SIE_INTAKE(self));
}

apr_off_t sie_file_read_block(sie_File *file, sie_Block *block)
{
    apr_off_t fpos;
    apr_size_t rsize;
    sie_int32 trailer[2];
    sie_int32 checksum;
    apr_size_t size;
    
    fpos = tell(file->fd);

    sie_debug((file->ctx, 10, "read_block starting at "
               "%"APR_OFF_T_FMT"\n", fpos));

    block->size = 0;

    size = SIE_HEADER_SIZE;
    if (apr_file_read(file->fd, block->data, &size) != APR_SUCCESS ||
        size != SIE_HEADER_SIZE)
        return -1;

    block->size = sie_ntoh32(block->data->size);
    block->group = sie_ntoh32(block->data->group);
    sie_debug((file->ctx, 10, "read_block: size %d group %d\n",
               block->size, block->group));
    if (sie_ntoh32(block->data->magic) != SIE_MAGIC)
        return -1;
    if (block->size < SIE_OVERHEAD_SIZE)
        return -1;
    if (block->size > 1048576) { /* KLUDGE arbitrary */
        int success = 1;
        apr_off_t current = 0;
        size_t read_size = 4;
        apr_off_t end;
        sie_int32 end_size;
        sie_debug((file->ctx, 10, "read_block: large block (%d) paranoia:\n",
                   block->size));
        if (apr_file_seek(file->fd, APR_CUR, &current) != APR_SUCCESS)
            return -1;
        end = current + block->size - 16;
        if (apr_file_seek(file->fd, APR_SET, &end) != APR_SUCCESS)
            return -1;  /* KLUDGE error? */
        if (apr_file_read(file->fd, &end_size, &read_size) != APR_SUCCESS)
            return -1;
        end_size = sie_ntoh32(end_size);
        sie_debug((file->ctx, 10, "read_block: end size = %d\n", end_size));
        if (end_size != block->size)
            success = 0;
        if (apr_file_seek(file->fd, APR_SET, &current) != APR_SUCCESS)
            return -1;
        if (!success)
            return -1;
    }
    sie_block_expand(block, block->size);

    sie_debug((file->ctx, 10, "read_block: magic %08x group %d size %u\n",
               sie_ntoh32(block->data->magic), block->group, block->size));
    
    size = rsize = (size_t)block->size - SIE_HEADER_SIZE;
    if (apr_file_read(file->fd, block->data->payload, &size) != APR_SUCCESS ||
        size != rsize)
        return -1;

    block->size -= SIE_OVERHEAD_SIZE;
    memcpy(trailer, block->data->payload + block->size, SIE_TRAILER_SIZE);
    sie_debug((file->ctx, 10, "read_block: trailersize %u\n",
               sie_ntoh32(trailer[1])));
    if (block->size != sie_ntoh32(trailer[1]) - SIE_OVERHEAD_SIZE)
        return -1;

    block->checksum = checksum = sie_ntoh32(trailer[0]);
    if (block->checksum) {
        sie_uint32 block_check = sie_crc((unsigned char *)block->data,
                                         block->size + SIE_HEADER_SIZE);
        if (block->checksum != block_check) {
            sie_debug((file, 1, "read_block: checksum WRONG, %08x != %08x\n",
                       block->checksum, block_check));
            return -1;
        }
    }

    return fpos;
}

apr_off_t sie_file_read_block_offset(sie_File *file, sie_Block *block,
                                     apr_off_t offset)
{
    if (apr_file_seek(file->fd, APR_SET, &offset) != APR_SUCCESS)
        return -1;
    return sie_file_read_block(file, block);
}

static apr_off_t sie_file_read_indexed_block(
    sie_File *file, sie_Block *block,
    sie_id group, sie_File_Group_Index_Entry *entry)
{
    apr_off_t ret;
    sie_debug((file, 12, "read_indexed_block(group = %d, entry = %p):\n",
               group, entry));
    ret = sie_file_read_block_offset(file, block, entry->offset);
    sie_debug((file, 12, "  group = %d, block->group = %d\n",
               group, block->group));
    sie_debug((file, 12, "  entry->size = %d, block->size = %d\n",
               entry->size, block->size));
    if (group != block->group || entry->size != block->size)
        sie_errorf((file, "Indexing error while reading group "
                    "%"APR_SIZE_T_FMT" block at file offset "
                    "%"APR_UINT64_T_FMT".",
                    (size_t)group, entry->offset));
    if (ret < 0)
        sie_errorf((file, "Error reading data from group "
                    "%"APR_SIZE_T_FMT" block at file offset "
                    "%"APR_UINT64_T_FMT".",
                    (size_t)group, entry->offset));
    return ret;
}

apr_off_t sie_file_find_block_backward(sie_File *file, sie_Block *block,
                                       size_t max_search)
{
    apr_off_t start = tell(file->fd);
    apr_off_t cur = start;
    apr_off_t magic;
    apr_off_t offset;
    apr_off_t ret;

    sie_debug((file->ctx, 10, "find_block_backward starting at "
               "%"APR_OFF_T_FMT"\n", cur));

    while (start - cur < (apr_off_t)max_search) {
        if ((ret = sie_file_read_block_backward(file, block)) >= 0) {
            return ret;
        }
        offset = cur;
        if (apr_file_seek(file->fd, APR_SET, &offset) != APR_SUCCESS)
            return -1;
        magic = sie_file_search_backwards_for_magic(
            file, (size_t)(max_search - (start - cur)));
        cur = offset = magic - 8;
        if (apr_file_seek(file->fd, APR_SET, &offset) != APR_SUCCESS)
            return -1;
        ret = sie_file_read_block(file, block);
        cur = offset = magic - 8;
        if (apr_file_seek(file->fd, APR_SET, &offset) != APR_SUCCESS)
            return -1;
        if (ret >= 0)
            return ret;
    }

    sie_debug((file->ctx, 1, "find_block_backward couldn't find a"
               "block in %"APR_SIZE_T_FMT" bytes\n", max_search));

    return -1;
}

apr_off_t sie_file_read_block_backward(sie_File *file, sie_Block *block)
{
    sie_uint32 size;
    apr_off_t offset;
    apr_size_t read_size;
    apr_off_t ret;

    sie_debug((file->ctx, 10, "read_block_backward starting at "
               "%"APR_OFF_T_FMT"\n", tell(file->fd)));

    offset = -4;
    if (apr_file_seek(file->fd, APR_CUR, &offset) != APR_SUCCESS)
        return -1;

    read_size = 4;
    if (apr_file_read(file->fd, &size, &read_size) != APR_SUCCESS ||
        read_size != 4)
        return -1;

    size = sie_ntoh32(size);
    if (size < SIE_OVERHEAD_SIZE)
        return -1;

    offset = -(apr_off_t)size;
    if (apr_file_seek(file->fd, APR_CUR, &offset) != APR_SUCCESS)
        return -1;

    ret = sie_file_read_block(file, block);

    if (ret == -1)
        return -1;

    offset = -(apr_off_t)size;
    if (apr_file_seek(file->fd, APR_CUR, &offset) != APR_SUCCESS)
        return -1;

    return ret;
}

apr_off_t sie_file_search_backwards_for_magic(sie_File *file,
                                              size_t max_search)
{
    apr_off_t step = 1020;
    unsigned char buf[1024];
    apr_off_t offset;
    apr_size_t read_size;
    apr_off_t count = 0;
    apr_off_t cur = tell(file->fd);
    size_t i;

    sie_debug((file->ctx, 4, "search_backwards_for_magic starting at "
               "%"APR_OFF_T_FMT"\n", cur));

    cur -= 4;
    while (cur > 0 && count < (apr_off_t)max_search) {
        if (cur < step) {
            count += cur;
            step = cur;
            cur = 0;
        } else {
            count += step;
            cur -= step;
        }

        sie_debug((file->ctx, 4, "  reading %"APR_OFF_T_FMT" "
                   "at %"APR_OFF_T_FMT", searching "
                   "for magic\n", step + 4, cur));

        offset = cur;
        if (apr_file_seek(file->fd, APR_SET, &offset) != APR_SUCCESS)
            return -1;

        read_size = (apr_size_t)step + 4;
        if (apr_file_read(file->fd, buf, &read_size) != APR_SUCCESS ||
            read_size != step + 4)
            return -1;

        for (i = (size_t)step; i > 0; i--) {
            if (buf[i]     == 0x51 && buf[i + 1] == 0xED &&
                buf[i + 2] == 0xA7 && buf[i + 3] == 0xA0) {
                cur += i;
                sie_debug((file->ctx, 4, 
                           "  found magic at %"APR_OFF_T_FMT"!\n", cur));
                return cur;
            }
        }
    }

    sie_debug((file->ctx, 1, "  magic not found in %"APR_OFF_T_FMT" bytes\n", 
               count));

    return -1;
}

void *sie_file_get_group_handle(sie_File *self, sie_id group)
{
    return get_group_index(self, group);
}

size_t sie_file_get_group_num_blocks(sie_File *self, void *group_handle)
{
    sie_File_Group_Index *gi = group_handle;
    return sie_vec_size(gi->entries);
}

sie_uint64 sie_file_get_group_num_bytes(sie_File *self, void *group_handle)
{
    sie_File_Group_Index *gi = group_handle;
    return gi->payload_size;
}

sie_uint32 sie_file_get_group_block_size(sie_File *self, void *group_handle,
                                         size_t entry)
{
    sie_File_Group_Index *gi = group_handle;
    return gi->entries[entry].size;
}

void sie_file_read_group_block(sie_File *self, void *group_handle,
                               size_t entry, sie_Block *block)
{
    sie_File_Group_Index *gi = group_handle;
    sie_file_read_indexed_block(self, block, gi->group,
                                &gi->entries[entry]);
}

int sie_file_is_group_closed(sie_File *self, void *group_handle)
{
    return 1;
}

void sie_file_get_unindexed_blocks(sie_File *self, char **index_buf,
                                   size_t *index_size)
{
    sie_Block *block = sie_autorelease(sie_block_new(self));
    char *buf = sie_malloc(self, self->num_unindexed * 12);
    apr_off_t offset = self->first_unindexed;
    size_t i;
    sie_cleanup_push(self, free, buf);
    if (self->num_unindexed) {
        if (apr_file_seek(self->fd, APR_SET, &offset) != APR_SUCCESS)
            sie_errorf((self, "Couldn't seek to initial position"));
        for (i = 0; i < self->num_unindexed; ++i) {
            offset = sie_file_read_block(self, block);
            sie_set_uint64(buf + (i * 12), sie_hton64(offset));
            sie_set_uint32(buf + (i * 12) + 8, sie_hton32(block->group));
        }
    }
    *index_buf = buf;
    *index_size = self->num_unindexed * 12;
    sie_cleanup_pop(self, buf, 0);
    sie_cleanup_pop(self, block, 1);
}

SIE_CLASS(sie_File, sie_Intake,
          SIE_MDEF(sie_destroy, sie_file_destroy)
          SIE_MDEF(sie_get_group_handle, sie_file_get_group_handle)
          SIE_MDEF(sie_get_group_num_blocks, sie_file_get_group_num_blocks)
          SIE_MDEF(sie_get_group_num_bytes, sie_file_get_group_num_bytes)
          SIE_MDEF(sie_get_group_block_size, sie_file_get_group_block_size)
          SIE_MDEF(sie_read_group_block, sie_file_read_group_block)
          SIE_MDEF(sie_is_group_closed, sie_file_is_group_closed));

SIE_CONTEXT_OBJECT_API_NEW_FN(sie_file_stream_new, sie_File_Stream, self,
                              ctx_obj, (void *ctx_obj, const char *name),
                              sie_file_stream_init(self, ctx_obj, name));

void sie_file_stream_init(sie_File_Stream *self, void *ctx_obj,
                          const char *name)
{
    sie_intake_init(SIE_INTAKE(self), ctx_obj);
    shared_init(SIE_FILE(self), name, APR_WRITE | APR_CREATE | APR_TRUNCATE);
    self->block = sie_block_new(self);
}

void sie_file_stream_destroy(sie_File_Stream *self)
{
    sie_release(self->block);
    sie_file_destroy(SIE_FILE(self));
}

static void add_block(void *self_v, sie_Block *block)
{
    sie_File_Stream *self = self_v;
    sie_File *file = SIE_FILE(self);
    apr_off_t offset = 0;
    apr_size_t size = block->size;
    char strerror_buf[256];
    apr_status_t status;

    /* write */
    if (apr_file_seek(file->fd, APR_END, &offset) != APR_SUCCESS)
        sie_errorf((file, "Seek to end of file failed"));
    status = apr_file_write(file->fd, block->data, &size); 
    if (status != APR_SUCCESS || size != block->size)
        sie_errorf((file, "Couldn't write block: %s",
                    apr_strerror(status, strerror_buf, sizeof(strerror_buf))));
    
    index_add(file, block->group, offset,
              block->size - SIE_OVERHEAD_SIZE);
    if (block->group == 0)
        sie_xml_definition_add_string(self->parent.parent.xml,
                                      block->data->payload,
                                      block->size - SIE_OVERHEAD_SIZE);
}

size_t sie_file_stream_add_stream_data(sie_File_Stream *self,
                                       const void *data_v,
                                       size_t size)
{
    return sie_stream_add_data_guts(self, self->block, &self->read,
                                    add_block, data_v, size);
}

int sie_file_stream_is_group_closed(sie_File *self, void *group_handle)
{
    if (group_handle) {
        sie_File_Group_Index *gi = group_handle;
        return gi->closed;
    } else {
        return 0;
    }
}

SIE_CLASS(sie_File_Stream, sie_File,
          SIE_MDEF(sie_destroy, sie_file_stream_destroy)
          SIE_MDEF(sie_add_stream_data, sie_file_stream_add_stream_data)
          SIE_MDEF(sie_is_group_closed, sie_file_stream_is_group_closed));
