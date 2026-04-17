/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

typedef struct _sie_Stream_Group_Index_Entry sie_Stream_Group_Index_Entry;
typedef struct _sie_Stream_Group_Index sie_Stream_Group_Index;
typedef struct _sie_Stream sie_Stream;

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_STREAM_H
#define SIE_STREAM_H

#include <stdio.h>

struct _sie_Stream_Group_Index_Entry {
    void *ptr;
    sie_uint32 size;
};

struct _sie_Stream_Group_Index {
    sie_Context *ctx;
    sie_id group;
    sie_vec_declare(entries, sie_Stream_Group_Index_Entry);
    sie_uint64 payload_size;
    size_t base;
    size_t first;
    int closed;
};

struct _sie_Stream {
    sie_Intake parent;
    sie_Id_Map *group_indexes;
    sie_Block *block;
    size_t read;
};
SIE_CLASS_DECL(sie_Stream);
#define SIE_STREAM(p) SIE_SAFE_CAST(p, sie_Stream)

SIE_DECLARE(sie_Stream *) sie_stream_new(void *ctx_obj);
SIE_DECLARE(void) sie_stream_init(sie_Stream *self, void *ctx_obj);
SIE_DECLARE(void) sie_stream_destroy(sie_Stream *self);

typedef void (sie_Add_Block_Fn)(void *self, sie_Block *block);

SIE_DECLARE(size_t) sie_stream_add_data_guts(
    void *self, sie_Block *block, size_t *read,
    sie_Add_Block_Fn *add_block_fn, const void *data_v, size_t size);

SIE_DECLARE(size_t) sie_stream_add_stream_data(
    sie_Stream *self, const void *data_v, size_t size);

SIE_DECLARE(void *) sie_stream_get_group_handle(sie_Stream *self,
                                                sie_id group);
SIE_DECLARE(size_t) sie_stream_get_group_num_blocks(sie_Stream *self,
                                                    void *group_handle);
SIE_DECLARE(sie_uint64) sie_stream_get_group_num_bytes(sie_Stream *self,
                                                       void *group_handle);
SIE_DECLARE(sie_uint32) sie_stream_get_group_block_size(sie_Stream *self,
                                                        void *group_handle,
                                                        size_t entry);
SIE_DECLARE(void) sie_stream_read_group_block(
    sie_Stream *self, void *group_handle, size_t entry, sie_Block *block);
SIE_DECLARE(int) sie_stream_is_group_closed(sie_Stream *self,
                                            void *group_handle);

#endif

#endif
