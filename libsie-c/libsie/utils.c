/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#include "sie_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sie_utils.h"
#include "sie_vec.h"

int sie_scan_number(const char *string, double *result, int *consumed)
{
    int d_p, i_p;
    double d;
    int i;
    int n_d, n_i;

    d_p = sscanf(string, "%lf%n", &d, &n_d) == 1;
    i_p = sscanf(string, "%i%n", &i, &n_i) == 1;

    if (!(d_p || i_p))
        return 0;

    if (d_p && !i_p) {
        *result = d;
        *consumed = n_d;
    } else if (i_p && !d_p) {
        *result = i;
        *consumed = n_i;
    } else if (d == 0.0) {
        *result = i;
        *consumed = n_i;
    } else {
        *result = d;
        *consumed = n_d;
    }
    
    return 1;
}

int sie_whitespace(const char *string)
{
    size_t len = strlen(string);
    size_t cur;
    for (cur = 0; cur < len; cur++) {
        char here = string[cur];
        if (!(here == ' ' || here == '\t' || here == '\r' || here == '\n'))
            break;
    }
    return (int)cur;
}

void sie_chomp(char *string)
{
    while (string[strlen(string) - 1] == '\n') {
        string[strlen(string) - 1] = 0;
    }
}

void sie_add_char_to_utf8_vec(void *self, char **vec, int ch)
{
    if (ch < 0x80) {
        sie_vec_push_back(*vec, ch);
    } else if (ch < 0x800) {
        sie_vec_push_back(*vec, 0xc0 | (ch >> 6));
        sie_vec_push_back(*vec, 0x80 | (ch & 0x3f));
    } else if (ch < 0x10000) {
        sie_vec_push_back(*vec, 0xe0 | (ch >> 12));
        sie_vec_push_back(*vec, 0x80 | ((ch >> 6) & 0x3f));
        sie_vec_push_back(*vec, 0x80 | (ch & 0x3f));
    } else {
        sie_vec_push_back(*vec, 0xf0 | (ch >> 18));
        sie_vec_push_back(*vec, 0x80 | ((ch >> 12) & 0x3f));
        sie_vec_push_back(*vec, 0x80 | ((ch >> 6) & 0x3f));
        sie_vec_push_back(*vec, 0x80 | (ch & 0x3f));
    }
}

void sie_free(void *ptr)
{
    free(ptr);
}

void sie_system_free(void *ptr)
{
    free(ptr);
}

sie_uint64 sie_hton64(sie_uint64 value)
{
#ifdef SIE_LITTLE_ENDIAN
    return sie_byteswap_64(value);
#else
    return value;
#endif
}

sie_uint64 sie_ntoh64(sie_uint64 value)
{
#ifdef SIE_LITTLE_ENDIAN
    return sie_byteswap_64(value);
#else
    return value;
#endif
}

int sie_strtoint(void *ctx_obj, const char *string)
{
    char *end;
    long val_long = strtol(string, &end, 10);
    int val = val_long;
    sie_assertf(end != string && *end == 0 && val_long == val,
                (ctx_obj, "The string '%s' is not a valid int.", string));
    return val;
}

sie_id sie_strtoid(void *ctx_obj, const char *string)
{
    char *end;
    unsigned long id_long = strtoul(string, &end, 10);
    sie_id id = id_long;
    sie_assertf(end != string && *end == 0 && id_long == id,
                (ctx_obj, "The string '%s' is not a valid ID.", string));
    return id;
}

size_t sie_strtosizet(void *ctx_obj, const char *string)
{
    char *end;
    unsigned long val_long = strtoul(string, &end, 10);
    size_t val = val_long;
    sie_assertf(end != string && *end == 0 && val_long == val,
                (ctx_obj, "The string '%s' is not a valid size_t.", string));
    return val;
}
