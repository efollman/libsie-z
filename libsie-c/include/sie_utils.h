/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_UTILS_H
#define SIE_UTILS_H

SIE_DECLARE(int) sie_scan_number(const char *string, double *result,
                                 int *consumed);
SIE_DECLARE(int) sie_whitespace(const char *string);
SIE_DECLARE(void) sie_chomp(char *string);
SIE_DECLARE(void) sie_add_char_to_utf8_vec(void *self, char **vec, int ch);

SIE_DECLARE(void) sie_free(void *ptr);
SIE_DECLARE(void) sie_system_free(void *ptr);

SIE_DECLARE(int) sie_strtoint(void *ctx_obj, const char *string);
SIE_DECLARE(sie_id) sie_strtoid(void *ctx_obj, const char *string);
SIE_DECLARE(size_t) sie_strtosizet(void *ctx_obj, const char *string);

#endif

#endif
