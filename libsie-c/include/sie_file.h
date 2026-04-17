/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

typedef struct _sie_File_Group_Index_Entry sie_File_Group_Index_Entry;
typedef struct _sie_File_Group_Index sie_File_Group_Index;
typedef struct _sie_File sie_File;
typedef struct _sie_File_Stream sie_File_Stream;

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_FILE_H
#define SIE_FILE_H

#include "sie_uthash.h"
#include <stdio.h>

struct _sie_File_Group_Index_Entry {
    sie_uint64 offset;
    sie_uint32 size;
};

struct _sie_File_Group_Index {
    sie_id group;
    sie_vec_declare(entries, sie_File_Group_Index_Entry);
    sie_uint64 payload_size;
    int closed;
    size_t scan_num_blocks;
    UT_hash_handle hh;
};

struct _sie_File {
    sie_Intake parent;
    sie_Context *ctx;
    apr_pool_t *pool;
    apr_file_t *fd;
    sie_File_Group_Index *group_indexes;
    apr_off_t first_unindexed;
    size_t num_unindexed;
    apr_off_t last_offset;
    sie_id highest_group;
};
SIE_CLASS_DECL(sie_File);
#define SIE_FILE(p) SIE_SAFE_CAST(p, sie_File)

SIE_DECLARE(sie_File *) sie_file_open(void *ctx_obj, const char *name);
SIE_DECLARE(void) sie_file_init(sie_File *self, void *ctx_obj,
                                const char *name);
SIE_DECLARE(void) sie_file_destroy(sie_File *self);

SIE_DECLARE(sie_File *) sie_file_barebones_new(void *ctx_obj, const char *name);
SIE_DECLARE(void) sie_file_barebones_init(sie_File *self, void *ctx_obj,
                                          const char *name);

SIE_DECLARE(apr_off_t) sie_file_read_block(sie_File *file, sie_Block *block);
SIE_DECLARE(apr_off_t) sie_file_read_block_offset(
    sie_File *file, sie_Block *block, apr_off_t offset);
SIE_DECLARE(apr_off_t) sie_file_find_block_backward(
    sie_File *file, sie_Block *block, size_t max_search);
SIE_DECLARE(apr_off_t) sie_file_read_block_backward(
    sie_File *file, sie_Block *block);
SIE_DECLARE(apr_off_t) sie_file_search_backwards_for_magic(
    sie_File *file, size_t max_search);

SIE_DECLARE(int) sie_file_is_sie(void *ctx_obj, const char *name);

SIE_DECLARE(void *) sie_file_get_group_handle(sie_File *self, sie_id group);
SIE_DECLARE(size_t) sie_file_get_group_num_blocks(sie_File *self,
                                                  void *group_handle);
SIE_DECLARE(sie_uint64) sie_file_get_group_num_bytes(sie_File *self,
                                                    void *group_handle);
SIE_DECLARE(sie_uint32) sie_file_get_group_block_size(sie_File *self,
                                                      void *group_handle,
                                                      size_t entry);
SIE_DECLARE(void) sie_file_read_group_block(
    sie_File *self, void *group_handle, size_t entry, sie_Block *block);
SIE_DECLARE(int) sie_file_is_group_closed(sie_File *self,
                                          void *group_handle);

SIE_DECLARE(void) sie_file_get_unindexed_blocks(
    sie_File *self, char **index_buf, size_t *index_size);

typedef void (sie_File_Group_Foreach_Fn)(
    sie_id id, sie_File_Group_Index *value, void *extra);

SIE_DECLARE(void) sie_file_group_foreach(
    sie_File *self, sie_File_Group_Foreach_Fn *fn, void *extra);

struct _sie_File_Stream {
    sie_File parent;
    sie_Block *block;
    size_t read;
};
SIE_CLASS_DECL(sie_File_Stream);
#define SIE_FILE_STREAM(p) SIE_SAFE_CAST(p, sie_File_Stream)

SIE_DECLARE(sie_File_Stream *) sie_file_stream_new(void *ctx_obj,
                                                   const char *name);
SIE_DECLARE(void) sie_file_stream_init(sie_File_Stream *self, void *ctx_obj,
                                       const char *name);
SIE_DECLARE(void) sie_file_stream_destroy(sie_File_Stream *self);

SIE_DECLARE(size_t) sie_file_stream_add_stream_data(
    sie_File_Stream *self, const void *data_v, size_t size);

#endif

#endif
