/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

typedef struct _sie_Intake sie_Intake;

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_INTAKE_H
#define SIE_INTAKE_H

struct _sie_Intake {
    sie_Ref parent;
    sie_XML_Definition *xml;
};
SIE_CLASS_DECL(sie_Intake);
#define SIE_INTAKE(p) SIE_SAFE_CAST(p, sie_Intake)

SIE_DECLARE(void) sie_intake_init(sie_Intake *self, void *ctx_obj);
SIE_DECLARE(void) sie_intake_destroy(sie_Intake *self);

SIE_DECLARE(sie_Channel *) sie_intake_get_channel(sie_Intake *intake,
                                                  sie_id id);
SIE_DECLARE(sie_Test *) sie_intake_get_test(sie_Intake *intake, sie_id id);

SIE_DECLARE(sie_Iterator *) sie_intake_get_all_channels(sie_Intake *self);
SIE_DECLARE(sie_Iterator *) sie_intake_get_channels(sie_Intake *self);

SIE_DECLARE(sie_Iterator *) sie_intake_get_all_tests(sie_Intake *self);
SIE_DECLARE(sie_Iterator *) sie_intake_get_tests(sie_Intake *self);

SIE_DECLARE(sie_Iterator *) sie_intake_get_tags(sie_Intake *self);

SIE_DECLARE(void *) sie_get_group_handle(void *self, sie_id group);
SIE_METHOD_DECL(sie_get_group_handle);

SIE_DECLARE(size_t) sie_get_group_num_blocks(void *self, void *group_handle);
SIE_METHOD_DECL(sie_get_group_num_blocks);

SIE_DECLARE(sie_uint64) sie_get_group_num_bytes(void *self,
                                                void *group_handle);
SIE_METHOD_DECL(sie_get_group_num_bytes);

SIE_DECLARE(sie_uint32) sie_get_group_block_size(void *self,
                                                 void *group_handle,
                                                 size_t entry);
SIE_METHOD_DECL(sie_get_group_block_size);

SIE_DECLARE(void) sie_read_group_block(void *self, void *group_handle,
                                       size_t index, sie_Block *block);
SIE_METHOD_DECL(sie_read_group_block);

SIE_DECLARE(int) sie_is_group_closed(void *self, void *group_handle);
SIE_METHOD_DECL(sie_is_group_closed);

SIE_DECLARE(size_t) sie_add_stream_data(void *self, const void *data,
                                        size_t size);
SIE_METHOD_DECL(sie_add_stream_data);

#endif

#endif
