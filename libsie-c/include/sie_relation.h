/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

#elif defined(SIE_INCLUDE_BODIES)

#ifndef _SIE_RELATION_H
#define _SIE_RELATION_H

#include <stdio.h>
#include <stdarg.h>

typedef struct _sie_Relation sie_Relation;
typedef struct _sie_Parameter sie_Parameter;

struct _sie_Parameter {
    ssize_t name_off;
    ssize_t name_len;
    ssize_t value_off;
    ssize_t value_len;
};

struct _sie_Relation {
    ssize_t count;
    ssize_t max;
    ssize_t size;
    ssize_t endp;
    sie_Parameter params[1];
};

#define sie_rel_count(x) ((x) ? (x)->count : 0) 
#define sie_rel_idx_name(x, i) \
  (((i) < sie_rel_count(x)) \
    ? (((x)->params[i].name_off) \
      ? ((char *)(x) + (x)->params[i].name_off) : (char *)"") \
    : (char *)"" )
#define sie_rel_idx_value(x, i) \
  (((i) < sie_rel_count(x)) \
    ? (((x)->params[i].value_off) \
      ? ((char *)(x) + (x)->params[i].value_off) : (char *)"") \
    : (char *)"")
#define sie_rel_idx_name_size(x, i) ((x)->params[i].name_len)
#define sie_rel_idx_value_size(x, i) ((x)->params[i].value_len)
#define sie_rel_idx_set_name_size(r, i, size) ((r)->params[i].name_len = (size))
#define sie_rel_idx_set_value_size(r, i, size) ((r)->params[i].value_len = (size))

#define CRLF "\015\012"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

SIE_DECLARE(sie_Relation *) sie_rel_split_string(char *q, ssize_t len,
    char *assign, char *delimit, char *whitespace, ssize_t raw);
#define SIE_REL_SPLIT_CGI 0
#define SIE_REL_SPLIT_RAW 1
SIE_DECLARE(sie_Relation *) sie_rel_decode_query_string(char *q, ssize_t len);
SIE_DECLARE(sie_Relation *) sie_rel_decode_query_file(char *filename);
SIE_DECLARE(sie_Relation *) sie_rel_clone(sie_Relation *d);
SIE_DECLARE(sie_Relation *) sie_rel_merge(sie_Relation *d1, sie_Relation *d2);
SIE_DECLARE(sie_Relation *) sie_rel_merge_multi(ssize_t count, ...);
SIE_DECLARE(ssize_t) sie_rel_name_index(sie_Relation *rel, const char *name);
SIE_DECLARE(ssize_t) sie_rel_value_index(sie_Relation *rel, const char *name);
SIE_DECLARE(char *) sie_rel_value(sie_Relation *rel, const char *name);
SIE_DECLARE(char *) sie_rel_sized_value(sie_Relation *rel, const char *name, 
    ssize_t *len);
SIE_DECLARE(sie_Relation *) sie_rel_set_value(sie_Relation *rel,
    const char *name, char *v);
SIE_DECLARE(sie_Relation *) sie_rel_set_value_va(sie_Relation *rel,
    const char *name, char *fmt, va_list args);
SIE_DECLARE_NONSTD(sie_Relation *) sie_rel_set_valuef(sie_Relation *rel, 
    const char *name, char *fmt, ...)
    __gcc_attribute__ ((format(printf, 3, 4)));
SIE_DECLARE(sie_Relation *) sie_rel_set_raw(sie_Relation *rel, const void *name,
    ssize_t nlen, void *v, ssize_t vlen);
SIE_DECLARE(int) sie_rel_scan_value(sie_Relation *rel, const char *name,
    char *format, void *i);
SIE_DECLARE(int) sie_rel_short_value(sie_Relation *rel, const char *name, 
    short *i);
SIE_DECLARE(int) sie_rel_int_value(sie_Relation *rel, const char *name, int *i);
SIE_DECLARE(int) sie_rel_float_value(sie_Relation *rel, const char *name, 
    float *f);
SIE_DECLARE(int) sie_rel_double_value(sie_Relation *rel, const char *name, 
    double *d);
SIE_DECLARE(void) sie_rel_free(sie_Relation *rel);
SIE_DECLARE(sie_Relation *) sie_rel_new(ssize_t nitems, ssize_t len);
SIE_DECLARE(sie_Relation *) sie_rel_idx_set_name(sie_Relation *r, ssize_t idx, 
    const char *name, ssize_t len);
SIE_DECLARE(sie_Relation *) sie_rel_idx_set_value(sie_Relation *r, ssize_t idx, 
    const char *value, ssize_t len);
SIE_DECLARE(void) sie_rel_idx_delete(sie_Relation *rel, ssize_t idx);
SIE_DECLARE(void) sie_rel_clear(sie_Relation *rel);
SIE_DECLARE(void) sie_rel_dump(sie_Relation *rel, const char *tag, FILE *fp);
SIE_DECLARE(void) sie_rel_diagnose(sie_Relation *rel, const char *name,
    FILE *fp);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _SIE_RELATION_H */

#endif
