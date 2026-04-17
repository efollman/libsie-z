/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

typedef struct _sie_Sifter sie_Sifter;
typedef struct _sie_Sifter_Map_Key sie_Sifter_Map_Key;
typedef struct _sie_Sifter_Map_Entry sie_Sifter_Map_Entry;

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_SIFTER_H
#define SIE_SIFTER_H

typedef void (sie_Sifter_Test_Sig_Fn)(sie_Sifter *sifter, sie_Test *test,
                                      void *user);

typedef enum _sie_Sifter_Map_Type {
    SIE_SIFTER_GROUP,
    SIE_SIFTER_TEST,
    SIE_SIFTER_CH,
    SIE_SIFTER_DECODER,
    SIE_SIFTER_TEST_BY_SIG,
    SIE_SIFTER_DECODER_BY_SIG,
    SIE_SIFTER_NUM_TYPES
} sie_Sifter_Map_Type;

struct _sie_Sifter_Map_Key {
    sie_Intake *intake;
    sie_id from_id;
};

struct _sie_Sifter_Map_Entry {
    void *key;
    size_t key_size;
    sie_id id;
    size_t start_block;
    size_t end_block;
    UT_hash_handle hh;
};

struct _sie_Sifter {
    sie_Context_Object parent;
    sie_Writer *writer;
    sie_Sifter_Test_Sig_Fn *test_sig_fn;
    void *user;
    sie_Sifter_Map_Entry *maps[SIE_SIFTER_NUM_TYPES];
    struct {
        sie_String *element;
        sie_String *attribute;
        sie_Sifter_Map_Type type;
    } *remappings;
};
SIE_CLASS_DECL(sie_Sifter);
#define SIE_SIFTER(p) SIE_SAFE_CAST(p, sie_Sifter)

SIE_DECLARE(sie_Sifter *) sie_sifter_new(sie_Writer *writer);
SIE_DECLARE(void) sie_sifter_init(sie_Sifter *self, sie_Writer *writer);
SIE_DECLARE(void) sie_sifter_destroy(sie_Sifter *self);

SIE_DECLARE(int) sie_sifter_add(sie_Sifter *self, void *ref);
SIE_DECLARE(int) sie_sifter_add_channel(sie_Sifter *self, void *ref,
                                        size_t start_block, size_t end_block);

SIE_DECLARE(void) sie_sifter_test_sig_fn(sie_Sifter *self,
                                         sie_Sifter_Test_Sig_Fn *fn,
                                         void *user);
SIE_DECLARE(void) sie_sifter_register_test(sie_Sifter *self, sie_Test *test,
                                           const void *key, size_t key_size);
                                          
SIE_DECLARE(void) sie_sifter_finish(sie_Sifter *self);

SIE_DECLARE(void) sie_sifter_xml(sie_Sifter *self, void *intake,
                                 sie_XML *node);

SIE_DECLARE(sie_uint64) sie_sifter_total_size(sie_Sifter *self);

#endif

#endif
