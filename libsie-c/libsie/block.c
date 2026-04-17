/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#include "sie_config.h"

#include <stdlib.h>

#include "sie_block.h"
#include "sie_context.h"

/* Table of CRCs of all 8-bit messages. */
static sie_uint32 crc_table[256];
   
/* Flag: has the table been computed? Initially false. */
static int crc_table_computed = 0;
   
/* Make the table for a fast CRC. */
static void make_crc_table(void)
{
    sie_uint32 c;
    int n, k;
   
    for (n = 0; n < 256; n++) {
        c = (sie_uint32) n;
        for (k = 0; k < 8; k++) {
            if (c & 1)
                c = 0xedb88320L ^ (c >> 1);
            else
                c = c >> 1;
        }
        crc_table[n] = c;
    }
    crc_table_computed = 1;
}
   
/* Update a running CRC with the bytes buf[0..len-1]--the CRC
   should be initialized to all 1's, and the transmitted value
   is the 1's complement of the final running CRC (see the
   crc() routine below)). */
   
static sie_uint32 update_crc(sie_uint32 crc, unsigned char *buf, size_t len)
{
    sie_uint32 c = crc;
    size_t n;
   
    if (!crc_table_computed)
        make_crc_table();
    for (n = 0; n < len; n++)
        c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
    return c;
}
   
/* Return the CRC of the bytes buf[0..len-1]. */
sie_uint32 sie_crc(unsigned char *buf, size_t len)
{
    return update_crc(0xffffffffL, buf, len) ^ 0xffffffffL;
}

SIE_CONTEXT_OBJECT_NEW_FN(sie_block_new, sie_Block, self, ctx_obj,
                          (void *ctx_obj), sie_block_init(self, ctx_obj));

void sie_block_init(sie_Block *self, void *ctx_obj)
{
    sie_context_object_init(SIE_CONTEXT_OBJECT(self), ctx_obj);
    self->max_size = sizeof(*self->data);
    self->data = sie_calloc(self, self->max_size);
}

void sie_block_destroy(sie_Block *self)
{
    free(self->data);
    sie_context_object_destroy(SIE_CONTEXT_OBJECT(self));
}

void sie_block_expand(sie_Block *self, sie_uint32 size)
{
    if (size > self->max_size) {
        void *new_data = sie_realloc(self, self->data, size);
        sie_assert(!size || new_data, self);
        self->data = new_data;
        self->max_size = size;
    }
}

SIE_CLASS(sie_Block, sie_Context_Object,
          SIE_MDEF(sie_destroy, sie_block_destroy));
