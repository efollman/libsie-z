/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

typedef struct _sie_Block sie_Block;

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_BLOCK_H
#define SIE_BLOCK_H

#define SIE_MAGIC         0x51EDA7A0
#define SIE_HEADER_SIZE   12
#define SIE_TRAILER_SIZE  8
#define SIE_OVERHEAD_SIZE (SIE_HEADER_SIZE + SIE_TRAILER_SIZE)

#define SIE_XML_GROUP     0
#define SIE_INDEX_GROUP   1

struct _sie_Block {
    sie_Context_Object parent;
    int group;
    sie_uint32 size;      /* Payload size */
    sie_uint32 max_size;  /* Amount allocated at block */
    sie_uint32 checksum;
    struct { /* KLUDGE need to be careful of padding issues */
        sie_uint32  size;  /* Payload size; block size on output */
        sie_uint32  group; /* Same on output */
        sie_uint32  magic; /* Same on output */
        char payload[8];    /* So sizeof(*block) == overhead */
    } *data;
};
SIE_CLASS_DECL(sie_Block);
#define SIE_BLOCK(p) SIE_SAFE_CAST(p, sie_Block)

SIE_DECLARE(sie_Block *) sie_block_new(void *ctx_obj);
SIE_DECLARE(void) sie_block_init(sie_Block *self, void *ctx_obj);
SIE_DECLARE(void) sie_block_destroy(sie_Block *self);
SIE_DECLARE(void) sie_block_expand(sie_Block *self, sie_uint32 size);

SIE_DECLARE(sie_uint32) sie_crc(unsigned char *buf, size_t len);

#endif

#endif
