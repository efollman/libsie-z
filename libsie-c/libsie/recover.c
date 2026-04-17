/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#define SIE_VEC_CONTEXT_OBJECT NULL /* recover is not customer-visible */

#include "sie_config.h"

struct index_summary {
    apr_off_t offset;
    size_t size;
    apr_off_t first_offset;
    apr_off_t last_offset;
};

struct recover_part {
    apr_off_t offset;
    apr_off_t size;
    sie_uint32 before;
    sie_uint32 after;
    size_t *before_glue_id;
    size_t *after_glue_id;
    int before_quick;
    int after_quick;
    struct index_summary *indexes;
};

struct glue_entry {
    struct {
        struct recover_part *before;
        struct recover_part *after;
        size_t size;
    } key;
    size_t glue_id;
    struct index_summary *indexes;
    UT_hash_handle hh;
};

#if 0
static void hex(char *buf, size_t size)
{
    while (size--)
        fprintf(stderr, "%02x ", (unsigned char)*buf++);
    fprintf(stderr, "\n");
}
#endif

static void summarize_index(sie_Block *block, struct index_summary *summary)
{
    char *last_offset = block->data->payload + block->size - 12;

    if (block->size < 12) {
        summary->size = 0;
        summary->first_offset = -1;
        summary->last_offset = -1;
    } else {
        summary->size = block->size + SIE_OVERHEAD_SIZE;
        summary->first_offset = sie_ntoh64(sie_get_uint64(block->data->payload));
        summary->last_offset = sie_ntoh64(sie_get_uint64(last_offset));
    }
}

static size_t glue(char *left, char *right, size_t size, sie_Block *block,
                   apr_off_t offset, apr_off_t mod)
{
    char *data;
    size_t result = (size_t)-1;
    size_t split;
    sie_int32 trailer[2];

    sie_block_expand(block, size);
    data = (char *)block->data;

    for (split = 1; split < size - 1; ++split) {
        if ((offset + split) % mod)
            continue;
        memcpy(data, left, split);
        memcpy(data + split, right + split, size - split);
        block->size = sie_ntoh32(block->data->size);
        if (block->size != size)
            continue;
        block->group = sie_ntoh32(block->data->group);
        if (sie_ntoh32(block->data->magic) != SIE_MAGIC)
            continue;

        block->size -= SIE_OVERHEAD_SIZE;

        memcpy(trailer, block->data->payload + block->size,
               SIE_TRAILER_SIZE);
        if (block->size != sie_ntoh32(trailer[1]) - SIE_OVERHEAD_SIZE)
            continue;

        block->checksum = sie_ntoh32(trailer[0]);
        if (1) { /* always assume checksum */
            sie_uint32 block_check = sie_crc((unsigned char *)block->data,
                                             block->size + SIE_HEADER_SIZE);
            if (block->checksum != block_check)
                continue;
        }

        result = split;
        break;
    }

    return result;
}

static void dump_gluerefs(size_t *glue_id)
{
    int first = 1;
    size_t *cur;
    printf("[");
    sie_vec_forall(glue_id, cur) {
        printf("%s%"APR_SIZE_T_FMT, first ? " " : ", ", *cur);
        first = 0;
    }
    printf(" ],\n");
}

static void dump_indexes(struct index_summary *indexes)
{
    int first = 1;
    struct index_summary *cur;
    printf("        \"indexes\": [");
    sie_vec_forall(indexes, cur) {
        printf("%s{ \"offset\": %"APR_OFF_T_FMT", "
               "\"size\": %"APR_SIZE_T_FMT", "
               "\"first_offset\": %"APR_OFF_T_FMT", "
               "\"last_offset\": %"APR_OFF_T_FMT" }",
               first ? "\n            " : ",\n            ",
               cur->offset, cur->size, cur->first_offset, cur->last_offset);
        first = 0;
    }
    printf(first ? " ],\n" : "\n        ],\n");
}

static void dump_json(struct recover_part *parts,
                      struct glue_entry *glue_entries)
{
    int first = 1;
    struct recover_part *part;
    struct glue_entry *entry;
    printf("{\n");
    printf("    \"parts\": [");
    sie_vec_forall(parts, part) {
        printf(first ? " {\n" : ", {\n");
        first = 0;
        printf("        \"before_glue\": ");
        dump_gluerefs(part->before_glue_id);
        printf("        \"file\": \"part_%020"APR_OFF_T_FMT".si\",\n",
               part->offset);
        printf("        \"after_glue\": ");
        dump_gluerefs(part->after_glue_id);
        printf("        \"offset\": %"APR_OFF_T_FMT",\n", part->offset);
        dump_indexes(part->indexes);
        printf("        \"size\": %"APR_OFF_T_FMT",\n", part->size);
        printf("        \"before_size\": %"APR_SIZE_T_FMT",\n",
               (size_t)part->before);
        printf("        \"after_size\": %"APR_SIZE_T_FMT"\n",
               (size_t)part->after);
        printf("    }");
    }
    printf(" ],\n");
    printf("    \"glue\": [");
    first = 1;
    for (entry = glue_entries; entry; entry = entry->hh.next) {
        if (entry->glue_id == -1)
            continue;
        printf(first ? " {\n" : ", {\n");
        first = 0;
        printf("        \"file\": \"glue_%"APR_SIZE_T_FMT".si\",\n",
               entry->glue_id);
        dump_indexes(entry->indexes);
        printf("        \"size\": %"APR_SIZE_T_FMT"\n", entry->key.size);
        printf("    }");
    }
    printf(" ]\n");
    printf("}");
}

static int read_part(sie_File *file, struct recover_part *part,
                     size_t sz, int do_before, char **buf,
                     apr_off_t *offset)
{
    apr_size_t read_size;
    apr_off_t off;
    char *wrbuf;

    if (sz < SIE_OVERHEAD_SIZE || sz > 1048576)
        return 0;

    if (do_before) {
        if (part->before_quick)
            return 0;
    } else {
        if (part->after_quick)
            return 0;
    }

    if (do_before)
        off = part->offset - sz;
    else
        off = part->offset + part->size;
    if (offset)
        *offset = off;

    sie_vec_set_size(*buf, sz);
    wrbuf = *buf;
    if (off < 0) {
        memset(wrbuf, 0xff, -off);
        sz -= -off;
        wrbuf += -off;
        off = 0;
    }

    if (apr_file_seek(file->fd, APR_SET, &off) != APR_SUCCESS)
        return 0;
    read_size = sz;
    sie_debug((file, 1, "read %"APR_SIZE_T_FMT" from %"APR_OFF_T_FMT"\n",
               read_size, off));
    if (apr_file_read(file->fd, wrbuf, &read_size) != APR_SUCCESS)
        return 0;
    if (read_size < sz)
        memset(wrbuf + read_size, 0xff, sz - read_size);
    sie_debug((file, 1, "got %"APR_SIZE_T_FMT" from %"APR_OFF_T_FMT"\n",
               read_size, off));

    return 1;
}

static void do_glue(sie_File *file, struct recover_part *part, char *my_buf,
                    struct recover_part *other, char *other_buf, size_t sz,
                    struct glue_entry **glue_entries, size_t *next_glue_id,
                    int do_before, sie_Block *block,
                    apr_off_t offset, apr_off_t mod)
{
    size_t result;
    struct glue_entry ge;
    struct glue_entry *entry;

    if (do_before) {
        ge.key.before = other;
        ge.key.after = part;
    } else {
        ge.key.before = part;
        ge.key.after = other;
    }
    ge.key.size = sz;
    ge.indexes = NULL;

    HASH_FIND(hh, *glue_entries, &ge.key, sizeof(ge.key), entry);
    if (entry) {
        sie_debug((file, 1, "  cache hit: %"APR_SIZE_T_FMT"\n",
                   entry->glue_id));
    } else {
        if (do_before)
            result = glue(other_buf, my_buf, sz, block, offset, mod);
        else
            result = glue(my_buf, other_buf, sz, block, offset, mod);
        if (result != (size_t)-1)
            ge.glue_id = (*next_glue_id)++;
        else
            ge.glue_id = result;
        sie_debug((file, 1, "  result: %"APR_SIZE_T_FMT" "
                   "(%"APR_SIZE_T_FMT")\n", ge.glue_id, result));

        if (result != -1) {
            apr_file_t *out;
            char outname[128];
            apr_size_t nbytes = block->size + SIE_OVERHEAD_SIZE;
            sprintf(outname, "glue_%"APR_SIZE_T_FMT".si",
                    ge.glue_id);
            sie_debug((file, 1, "opening %s\n", outname));
            sie_assertf(apr_file_open(
                            &out, outname,
                            APR_WRITE | APR_CREATE | APR_TRUNCATE |
                            APR_BINARY | APR_BUFFERED,
                            APR_OS_DEFAULT,
                            file->pool) == APR_SUCCESS,
                        (file, "couldn't open %s", outname));
            sie_assert(apr_file_write(out, block->data,
                                      &nbytes) == APR_SUCCESS &&
                       nbytes == block->size + SIE_OVERHEAD_SIZE,
                       file);
            sie_assert(apr_file_close(out) == APR_SUCCESS, file);
            out = NULL;

            if (block->group == 1) {
                struct index_summary summary = { 0 };
                summarize_index(block, &summary);
                sie_vec_push_back(ge.indexes, summary);
            }
        }

        entry = sie_malloc(file, sizeof(ge));
        memcpy(entry, &ge, sizeof(ge));
        HASH_ADD_KEYPTR(hh, *glue_entries, &entry->key,
                        sizeof(entry->key), entry);
    }

    if (entry->glue_id != -1) {
        if (do_before)
            sie_vec_push_back(part->before_glue_id, entry->glue_id);
        else
            sie_vec_push_back(part->after_glue_id, entry->glue_id);
    }
}

void sie_file_recover(void *ctx_obj, const char *filename, size_t mod)
{
    unsigned char buf[65536];
    size_t readptr = 0;
    size_t size;
    sie_File *file = sie_file_barebones_new(ctx_obj, filename);
    sie_Block *block = sie_block_new(file);
    apr_off_t fsize, offset;
    apr_file_t *out = NULL;
    ssize_t count = 0;
    char *my_buf = NULL;
    char *other_buf = NULL;
    struct recover_part *parts = NULL;
    sie_uint32 magic = sie_hton32(SIE_MAGIC);
    struct recover_part *part, *last_part = NULL;
    struct glue_entry *glue_entries = NULL;
    struct glue_entry *entry, *next;
    size_t next_glue_id = 0;

    sie_vec_set_size(my_buf, 0);
    sie_vec_set_size(other_buf, 0);

    fsize = 0;
    if (apr_file_seek(file->fd, APR_END, &fsize) != APR_SUCCESS)
        sie_errorf((file, "Seek to end of file failed"));
    offset = 0;
    if (apr_file_seek(file->fd, APR_SET, &offset) != APR_SUCCESS)
        sie_errorf((file, "Seek to beginning of file failed"));
    size = sizeof(buf);
    sie_progress_msg(file, "(Pass 1 of 3) Saving contiguous blocks...");
    sie_progress(file, offset, fsize);
    while (apr_file_read(file->fd, buf + readptr, &size) == APR_SUCCESS) {
        ssize_t i;
        size += readptr;
        for (i = 0; i < (ssize_t)size - 4; ++i) {
            if (*(sie_uint32 *)&buf[i] == magic) {
                int do_before = 0;
                struct recover_part part =
                    { 0, 0, 0, 0, NULL, NULL, 0, 0, NULL };
                apr_off_t saved_offset = offset;
                apr_off_t saved_fp = 0;
                apr_file_seek(file->fd, APR_CUR, &saved_fp);

                part.offset = offset = offset + i - 8;

                if (offset >= 4) {
                    offset -= 4;
                    do_before = 1;
                }
                if (apr_file_seek(file->fd, APR_SET, &offset) != APR_SUCCESS)
                    sie_errorf((file, "Seek to offset failed"));
                if (do_before) {
                    size_t size = 4;
                    sie_assert(apr_file_read(file->fd, &part.before,
                                             &size) == APR_SUCCESS &&
                               size == 4, file);
                    part.before = sie_ntoh32(part.before);
                    offset += 4;
                }
                while (sie_file_read_block(file, block) >= 0) {
                    size_t size = block->size + SIE_OVERHEAD_SIZE;
                    apr_size_t nbytes = size;
                    if (!out) {
                        char outname[128];
                        sprintf(outname, "part_%020"APR_OFF_T_FMT".si",
                                offset);
                        sie_debug((file, 1, "opening %s\n", outname));
                        sie_assertf(apr_file_open(
                                        &out, outname,
                                        APR_WRITE | APR_CREATE | APR_TRUNCATE |
                                        APR_BINARY | APR_BUFFERED,
                                        APR_OS_DEFAULT,
                                        file->pool) == APR_SUCCESS,
                                    (file, "couldn't open %s", outname));
                    }
                    if (block->group == 1) {
                        struct index_summary summary = { part.size };
                        summarize_index(block, &summary);
                        sie_vec_push_back(part.indexes, summary);
                    }
                    sie_assert(apr_file_write(out, block->data,
                                              &nbytes) == APR_SUCCESS
                               && nbytes == size, file);
                    offset += nbytes;
                    part.size += nbytes;
                    sie_progress(file, offset, fsize);
                }
                if (out) {
                    size_t size = 4;
                    apr_off_t new_offset = offset;
                    sie_assert(apr_file_close(out) == APR_SUCCESS, file);
                    out = NULL;
                    if (apr_file_seek(file->fd, APR_SET,
                                      &new_offset) != APR_SUCCESS)
                        sie_errorf((file, "Seek to offset failed"));
                    if (apr_file_read(file->fd, &part.after,
                                      &size) == APR_SUCCESS
                        && size == 4)
                        part.after = sie_ntoh32(part.after);
                    else
                        part.after = 0;
                    sie_vec_push_back(parts, part);

                    new_offset = offset;
                    if (apr_file_seek(file->fd, APR_SET,
                                      &new_offset) != APR_SUCCESS)
                        sie_errorf((file, "Seek to offset failed"));

                    readptr = 0;
                    goto restart_scan;
                } else {
                    offset = saved_offset;
                    apr_file_seek(file->fd, APR_SET, &saved_fp);
                }
            }
        }
        if (size > 4) {
            memmove(buf + size - 4, buf, 4);
            offset += size - 4;
            readptr = 4;
        } else {
            readptr = size;
        }
    restart_scan:
        size = sizeof(buf) - readptr;
        sie_progress(file, offset, fsize);
    }

    sie_progress_msg(file,
                     "(Pass 2 of 3) Gluing blocks together (ordered)...");
    count = sie_vec_size(parts);
    sie_progress(file, 0, count);
    sie_vec_forall(parts, part) {
        while (last_part) {
            sie_uint32 sz = last_part->after;

            if (!read_part(file, last_part, sz, 0, &my_buf, &offset))
                break;
            if (!read_part(file, part, sz, 1, &other_buf, NULL))
                break;

            sie_progress(file, part - parts, count);
            sie_debug((file, 1, "ordered glue part %"APR_SIZE_T_FMT" to "
                       "other %"APR_SIZE_T_FMT":\n",
                       last_part - parts, part - parts));
            
            do_glue(file, last_part, my_buf,
                    part, other_buf, sz,
                    &glue_entries, &next_glue_id,
                    0, block, offset, mod);

            if (last_part->after_glue_id) {
                sie_vec_push_back(part->before_glue_id,
                                  last_part->after_glue_id[0]);
                last_part->after_quick = 1;
                part->before_quick = 1;
            }
            break;
        }
        last_part = part;
    }

    sie_progress_msg(file, "(Pass 3 of 3) Gluing blocks together (full)...");
    count = sie_vec_size(parts) * sie_vec_size(parts) * 2;
    sie_progress(file, 0, count);
    sie_vec_forall(parts, part) {
        int do_before;
        for (do_before = 0; do_before < 2; ++do_before) {
            struct recover_part *other;
            sie_uint32 sz = do_before ? part->before : part->after;

            if (!read_part(file, part, sz, do_before, &my_buf, &offset))
                continue;

            sie_vec_forall(parts, other) {
                if (other == part)
                    continue;

                if (!read_part(file, other, sz, !do_before, &other_buf, NULL))
                    continue;

                sie_progress(file, (part - parts) * sie_vec_size(parts) * 2 +
                             sie_vec_size(parts) * do_before +
                             (other - parts), count);
                sie_debug((file, 1, "glue part %"APR_SIZE_T_FMT" %s to "
                           "other %"APR_SIZE_T_FMT":\n",
                           part - parts, do_before ? "before" : "after",
                           other - parts));

                do_glue(file, part, my_buf,
                        other, other_buf, sz,
                        &glue_entries, &next_glue_id,
                        do_before, block, offset, mod);
            }
        }
    }
    sie_progress(file, count, count);

    dump_json(parts, glue_entries);

    sie_vec_forall(parts, part) {
        sie_vec_free(part->before_glue_id);
        sie_vec_free(part->after_glue_id);
        sie_vec_free(part->indexes);
    }

    for (entry = glue_entries; entry; entry = next) {
        next = entry->hh.next;
        HASH_DELETE(hh, glue_entries, entry);
        sie_vec_free(entry->indexes);
        free(entry);
    }
    
    sie_vec_free(parts);
    sie_vec_free(my_buf);
    sie_vec_free(other_buf);

    sie_release(block);
    sie_release(file);
}
