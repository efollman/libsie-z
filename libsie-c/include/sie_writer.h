/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

typedef struct _sie_Writer sie_Writer;
typedef struct _sie_Writer_Map_Entry sie_Writer_Map_Entry;

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_WRITER_H
#define SIE_WRITER_H

typedef size_t (sie_Writer_Fn)(void *user, const char *data, size_t size);

typedef enum _sie_Writer_Id_Type {
    SIE_WRITER_ID_GROUP,
    SIE_WRITER_ID_TEST,
    SIE_WRITER_ID_CH,
    SIE_WRITER_ID_DECODER,
    SIE_WRITER_ID_NUM_TYPES
} sie_Writer_Map_Type;

struct _sie_Writer {
    sie_Context_Object parent;
    sie_Writer_Fn *writer_fn;
    void *user;
    char *xml_buf;
    char *index_buf;
    sie_uint64 offset;
    int do_index;
    sie_id next_ids[SIE_WRITER_ID_NUM_TYPES];
};
SIE_CLASS_DECL(sie_Writer);
#define SIE_WRITER(p) SIE_SAFE_CAST(p, sie_Writer)

SIE_DECLARE(sie_Writer *) sie_writer_new(void *ctx_obj,
                                         sie_Writer_Fn *writer_fn,
                                         void *user);

SIE_DECLARE(void) sie_writer_init(sie_Writer *self, void *ctx_obj,
                                  sie_Writer_Fn *writer_fn,
                                  void *user);

SIE_DECLARE(void) sie_writer_destroy(sie_Writer *self);

SIE_DECLARE(int) sie_writer_write_block(sie_Writer *self, sie_id group,
                                        const char *data, size_t size);

SIE_DECLARE(int) sie_writer_xml_string(sie_Writer *self,
                                       const char *data, size_t size);
SIE_DECLARE(int) sie_writer_xml_node(sie_Writer *self, sie_XML *node);
SIE_DECLARE(int) sie_writer_xml_header(sie_Writer *self);
SIE_DECLARE(int) sie_writer_flush_xml(sie_Writer *self);

SIE_DECLARE(sie_id) sie_writer_next_id(sie_Writer *self, int type);
SIE_DECLARE(void) sie_writer_prepare_append(sie_Writer *self, void *intake);

SIE_DECLARE(sie_uint64) sie_writer_total_size(sie_Writer *self,
                                              sie_uint64 addl_bytes,
                                              sie_uint64 addl_blocks);

#endif

#endif
