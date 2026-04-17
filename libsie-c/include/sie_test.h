/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

typedef struct _sie_Test sie_Test;

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_TEST_H
#define SIE_TEST_H

struct _sie_Test {
    sie_Ref parent;
    sie_Intake *intake;
    sie_XML *node;
    sie_id id;
};
SIE_CLASS_DECL(sie_Test);
#define SIE_TEST(p) SIE_SAFE_CAST(p, sie_Test)

SIE_DECLARE(sie_Test *) sie_test_new(void *intake, sie_XML *node);
SIE_DECLARE(void) sie_test_init(sie_Test *self, void *intake, sie_XML *node);
SIE_DECLARE(void) sie_test_destroy(sie_Test *self);

SIE_DECLARE(sie_Iterator *) sie_test_get_all_channels(sie_Test *self);
SIE_DECLARE(sie_Iterator *) sie_test_get_channels(sie_Test *self);

SIE_DECLARE(void) sie_test_dump(sie_Test *self, FILE *stream);
SIE_DECLARE(sie_Iterator *) sie_test_get_tags(sie_Test *self);
SIE_DECLARE(sie_id) sie_test_get_id(sie_Test *self);

SIE_DECLARE(void) sie_test_dump(sie_Test *self, FILE *stream);

#endif

#endif
