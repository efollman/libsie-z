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
#include <stdarg.h>

#include "sie_config.h"
#include "sie_relation.h"

sie_Relation *sie_rel_new(ssize_t nitems, ssize_t len)
{
    ssize_t head_size;
    ssize_t size;
    sie_Relation *rel;

    if (nitems == 0)
        nitems = 8; /* KLUDGE?? */
    head_size = sizeof(sie_Relation) + nitems * sizeof(sie_Parameter);
    size = head_size + len + 2 * nitems;
    if (!(rel = (sie_Relation *)calloc(1, size)))
        return rel;
    rel->count = 0;
    rel->max = nitems;
    rel->size = size;
    rel->endp = head_size;
    return rel;
}

void sie_rel_clear(sie_Relation *rel)
{
    rel->endp = sizeof(sie_Relation) + rel->max * sizeof(sie_Parameter);
    memset(rel->params, 0, sizeof(rel->params[0]) * rel->max);
    rel->count = 0;
}

static int _set_param_count(sie_Relation **d, ssize_t new_count)
{
    sie_Relation *rel = *d;
    char *src;
    char *dst;
    ssize_t i;
    ssize_t size_delta;
    ssize_t size;

    if (rel->max >= new_count)
        return 1;

    size_delta = sizeof(sie_Parameter) * (new_count - rel->max);
    if (!(rel = realloc(rel, rel->size + size_delta)))
        return 0;

    /* Move strings */
    src = (char *)&rel->params[rel->max];
    dst = (char *)&rel->params[new_count];
    size = rel->size - (src - (char *)rel);
    memmove(dst, src, size);
    memset(src, 0, size_delta); /* Zero out new params */

    /* Update stuff */
    rel->max = new_count;
    rel->size += size_delta;
    rel->endp += size_delta;

    /* Fixup sie_Parameters */
    for (i = 0; i < rel->count; i++) {
        if (rel->params[i].name_off)
            rel->params[i].name_off += size_delta;
        if (rel->params[i].value_off)
            rel->params[i].value_off += size_delta;
    }
    *d = rel;

    return 1;
}

static int _add_param(sie_Relation **dp, ssize_t idx, ssize_t *p_off,
    ssize_t *p_len, const char *name, ssize_t len)
{
    sie_Relation *d;

    if (!_set_param_count(dp, idx + 1))
        return 0;
    d = *dp;
    if (d->endp + len + 1 >= d->size) {
        /* KLUDGE inefficient - should double size if a concern */
        if (!(d = (sie_Relation *)realloc(d, d->size + len + 1)))
            return 0;
	d->size += len + 1;
	*dp = d;
    }
    *p_off = d->endp;
    *p_len = len;
    memcpy((char *)d + d->endp, name, len);
    *((char *)d + d->endp + len) = (char)0;
    d->endp += len + 1;
    return 1;
}

static int sie_rel_add_name(sie_Relation **dp, ssize_t idx,
    const char *name, ssize_t len)
{
    ssize_t p_off, p_len;

    if (!_add_param(dp, idx, &p_off, &p_len, name, len))
        return 0;
    (*dp)->params[idx].name_off = p_off;
    (*dp)->params[idx].name_len = p_len;
    if ((*dp)->count < idx + 1)
        (*dp)->count = idx + 1;
    return 1;
}

static int sie_rel_add_value(sie_Relation **dp, ssize_t idx,
    const char *name, ssize_t len)
{
    ssize_t p_off, p_len;

    if (!_add_param(dp, idx, &p_off, &p_len, name, len))
        return 0;
    (*dp)->params[idx].value_off = p_off;
    (*dp)->params[idx].value_len = p_len;
    if ((*dp)->count < idx + 1)
        (*dp)->count = idx + 1;
    return 1;
}

sie_Relation *sie_rel_idx_set_name(sie_Relation *r, ssize_t idx,
    const char *name, ssize_t len)
{
    sie_Relation *nr = r;
    if (sie_rel_add_name(&nr, idx, name, len))
        return nr;
    return NULL;
}

sie_Relation *sie_rel_idx_set_value(sie_Relation *r, ssize_t idx,
    const char *value, ssize_t len)
{
    sie_Relation *nr = r;
    if (sie_rel_add_value(&nr, idx, value, len))
        return nr;
    return NULL;
}

static void make_assign_table(char *table, char *stuffing)
{
    if (!table)
        return;
    memset(table, 0, 257);
    if (stuffing) {
        unsigned char *ucp = (unsigned char *)stuffing;
	while (*ucp)
	    table[*ucp++] = 1;
    }
}

static long hex_value(char *cp, ssize_t len)
{
    unsigned long value = 0;

    while (len--) {
        long c = *cp++;
        value <<= 4;
        if (c >= '0' && c <= '9')
	    value += c - '0';
        else if (c >= 'a' && c <= 'f')
	    value += c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
	    value += c - 'A' + 10;
	else
	    return -1;
    }
    return value;
}

sie_Relation *sie_rel_split_string(char *q, ssize_t len, char *assign,
    char *delimit, char *whitespace, ssize_t raw)
{
    sie_Relation *rel;
    ssize_t i;
    char *start_p;
    int is_param;
    char *cp;
    ssize_t vcount = 1;
    char is_assign[257];
    char is_delimit[257];
    char is_whitespace[257];

    if (!len)
        len = strlen(q);
    make_assign_table(is_assign, assign);
    make_assign_table(is_delimit, delimit);
    make_assign_table(is_whitespace, whitespace);
    is_delimit[256] = 1;

    i = len;
    cp = q;
    while (i--) {
        if (is_delimit[*(unsigned char *)cp])
	    ++vcount;
	++cp;
    }
    if (!(rel = sie_rel_new(vcount, len + 1)))
        return rel;

    rel->count = 0;
    cp = (char *)rel + rel->endp;
    start_p = cp;
    i = (len) ? len + 1 : 0;
    is_param = 1;

    while (i--) {
        int c = (i) ? *(unsigned char *)q : 256;
	if (is_assign[c]) {
	    if (is_param) {
	        rel->params[rel->count].name_off
	            = start_p - (char *)rel;
	        rel->params[rel->count].name_len = cp - start_p;
	        *cp++ = (char)0;
		rel->endp = cp - (char *)rel;
		start_p = cp;
		is_param = 0;
	    }
	} else if (is_delimit[c]) {
	    /* For a case like &foo& (no '=') set the name offset to 0 */
	    if (is_param)
	        rel->params[rel->count].name_off = 0;
	    rel->params[rel->count].value_off
	        = start_p - (char *)rel;
	    rel->params[rel->count].value_len = cp - start_p;
	    *cp++ = (char)0;
	    rel->endp = cp - (char *)rel;
	    start_p = cp;
	    is_param = 1;
	    ++rel->count;
	    /* Skip whitespace. KLUDGE?!? */
	    if (whitespace) {
	        while (i) {
		    if (!is_whitespace[((unsigned char *)q)[1]])
		        break;
		    ++q;
		    --i;
		}
	    }
	} else if (raw == SIE_REL_SPLIT_RAW) {
	    *cp++ = c;
	} else if (c == '%' && i >= 2) {
	    int hval = hex_value(q + 1, 2);
	    if (hval < 0) {
	        *cp++ = c;
	    } else {
	        *cp++ = hval;
	        q += 2;
	        i -= 2;
	    }
	} else if (c == '+') {	/* KLUDGE? General enough? */
	    *cp++ = ' ';
	} else {
	    *cp++ = c;
	}
	++q;
    }
    return rel;
}

sie_Relation *sie_rel_decode_query_string(char *q, ssize_t len)
{
    return sie_rel_split_string(q, len, "=", "&;", " \t\015\012",
        SIE_REL_SPLIT_CGI);
}

sie_Relation *sie_rel_decode_query_file(char *filename)
{
    FILE *fp;
    char *buf;
    size_t len;
    sie_Relation *rel;

    if (!(fp = fopen(filename, "r")))
        return NULL;
    (void)fseek(fp, 0L, SEEK_END);
    len = ftell(fp);
    (void)fseek(fp, 0L, SEEK_SET);
    if (!(buf = malloc(len)))
        return NULL;
    if (fread(buf, 1, len, fp) != len) {
	free(buf);
        return NULL;
    }
    fclose(fp);
    rel = sie_rel_decode_query_string(buf, len);
    free(buf);
    return rel;
}

sie_Relation *sie_rel_clone(sie_Relation *d)
{
    sie_Relation *clone;
    
    if (!d)
        return d;
    if (!(clone = (sie_Relation *)malloc(d->size)))
        return clone;
    memcpy(clone, d, d->size);
    return clone;
}

#if 0
sie_Relation *sie_rel_merge(sie_Relation *d1, sie_Relation *d2)
{
    sie_Relation *rel;
    ssize_t size;
    char *src;
    char *dest;
    ssize_t i;

    if (!d1)
        return sie_rel_clone(d2);
    else if (!d2)
        return sie_rel_clone(d1);

    size = d1->size + d2->size;
    if (!(rel = malloc(size)))
        return rel;
    rel->count = d1->count + d2->count;
    rel->max = rel->count;
    rel->size = size;

    /* Copy d1 sie_Parameter */
    dest = (char *)&rel->params[0];
    size = d1->count * sizeof(sie_Parameter);
    memcpy(dest, (char *)&d1->params[0], size);

    /* Copy d2 sie_Parameter */
    dest += size;
    size = d2->count * sizeof(sie_Parameter);
    memcpy(dest, (char *)&d2->params[0], size);

    /* Copy d1 strings */
    dest += size;
    src = (char *)&d1->params[d1->max];
    size = d1->size - (src - (char *)d1);
    memcpy(dest, src, size);

    /* Copy d2 strings */
    dest += size;
    src = (char *)&d2->params[d2->max];
    size = d2->size - (src - (char *)d2);
    memcpy(dest, src, size);

    rel->endp = dest + size - (char *)rel;

    /* Fixup sie_Parameters */
    
    rel->endp = 0;
    size = d2->count * sizeof(sie_Parameter);
    for (i = 0; i < rel->count; i++) {
        ssize_t endp;

        if (i >= d1->count)
	    size = d1->size - ((char *)&d1->params[0] - (char *)d1);
        if (rel->params[i].name_off)
            rel->params[i].name_off += size;
	endp = rel->params[i].name_off + rel->params[i].name_len;
	if (rel->endp < endp)
	    rel->endp = endp;
        if (rel->params[i].value_off)
            rel->params[i].value_off += size;
	endp = rel->params[i].value_off + rel->params[i].value_len;
	if (rel->endp < endp)
	    rel->endp = endp;
    }

    return rel;
}
#else
sie_Relation *sie_rel_merge(sie_Relation *d1, sie_Relation *d2)
{
    sie_Relation *rel;
    ssize_t i;
    ssize_t n = 0;

    if (!d1)
        return sie_rel_clone(d2);
    else if (!d2)
        return sie_rel_clone(d1);

    rel = sie_rel_new(d1->max + d2->max, d1->size + d2->size);
    if (!rel)
        return rel;

    for (i = 0; i < d1->count; i++) {
        rel = sie_rel_idx_set_name(rel, n,
            sie_rel_idx_name(d1, i), sie_rel_idx_name_size(d1, i));
        rel = sie_rel_idx_set_value(rel, n,
            sie_rel_idx_value(d1, i), sie_rel_idx_value_size(d1, i));
        ++n;
    }
    for (i = 0; i < d2->count; i++) {
        rel = sie_rel_idx_set_name(rel, n,
            sie_rel_idx_name(d2, i), sie_rel_idx_name_size(d2, i));
        rel = sie_rel_idx_set_value(rel, n,
            sie_rel_idx_value(d2, i), sie_rel_idx_value_size(d2, i));
        ++n;
    }
    return rel;
}
#endif

sie_Relation *sie_rel_merge_multi(ssize_t count, ...)
{
    va_list args;
    sie_Relation *merged = NULL;
    sie_Relation *dp;

    va_start(args, count);
    while (count--) {
        dp = va_arg(args, sie_Relation *);
        if (!merged && dp) {
            merged = sie_rel_clone(dp);
	} else if (dp) {
	    sie_Relation *nd = sie_rel_merge(merged, dp);
	    if (!nd)
	        return nd;
	    sie_rel_free(merged);
	    merged = nd;
	}
    }
    va_end(args);
    return merged;
}

void sie_rel_idx_delete(sie_Relation *rel, ssize_t idx)
{
    if (idx < 0 || idx >= rel->count)
        return;
    if (idx < rel->count - 1) {
        memmove(rel->params + idx, rel->params + idx + 1, 
	    (rel->count - idx - 1) * sizeof(rel->params[0]));
    }
    --rel->count;
}

ssize_t sie_rel_name_index(sie_Relation *rel, const char *name)
{
    ssize_t i;
    if (!rel)
        return -1;
    for (i = 0; i < rel->count; i++)
        if (!strcmp(name, sie_rel_idx_name(rel, i)))
	    return i;
    return -1;
}

ssize_t sie_rel_value_index(sie_Relation *rel, const char *name)
{
    ssize_t i;
    if (!rel)
        return -1;
    for (i = 0; i < rel->count; i++)
        if (!strcmp(name, sie_rel_idx_value(rel, i)))
	    return i;
    return -1;
}

char *sie_rel_value(sie_Relation *rel, const char *name)
{
    return sie_rel_sized_value(rel, name, (ssize_t *)NULL);
}

char *sie_rel_sized_value(sie_Relation *rel, const char *name, ssize_t *len)
{
    ssize_t i = sie_rel_name_index(rel, name);

    if (i < 0)
        return (char *)0;
    if (len)
	*len = sie_rel_idx_value_size(rel, i);
    return sie_rel_idx_value(rel, i);
}

sie_Relation *sie_rel_set_value(sie_Relation *rel, const char *name, char *v)
{
    ssize_t olen = 0;
    void *p = sie_rel_sized_value(rel, name, &olen);
    ssize_t len = strlen(v);

    if (!p) {
        ssize_t idx = rel->count;
	rel = sie_rel_idx_set_name(rel, idx, name, strlen(name));
	rel = sie_rel_idx_set_value(rel, idx, v, len);
    } else {
        ssize_t idx = sie_rel_name_index(rel, name);
        if (len > olen) {
            rel = sie_rel_idx_set_value(rel, idx, v, len);
        } else {
            strcpy(p, v);
            sie_rel_idx_set_value_size(rel, idx, len);
        }
    }
    return rel;
}

sie_Relation *sie_rel_set_value_va(sie_Relation *rel, const char *name,
    char *fmt, va_list args)
{
    char *buf = NULL;
    ssize_t len;
    va_list args_copy;

    va_copy(args_copy, args);
    len = apr_vsnprintf(buf, 0, fmt, args_copy);
    va_end(args_copy);
    buf = alloca(len + 1);
    len = apr_vsnprintf(buf, len + 1, fmt, args);
    rel = sie_rel_set_value(rel, name, buf);
    return rel;
}

sie_Relation *sie_rel_set_valuef(sie_Relation *rel, const char *name, char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    rel = sie_rel_set_value_va(rel, name, fmt, args);
    va_end(args);
    return rel;
}

sie_Relation *sie_rel_set_raw(sie_Relation *rel, const void *name, ssize_t nlen,
    void *v, ssize_t vlen)
{
    ssize_t i;

    if (!rel)
        return NULL;
    for (i = 0; i < rel->count; i++) {
        if (rel->params[i].name_off && sie_rel_idx_name_size(rel, i) == nlen &&
            !memcmp(name, sie_rel_idx_name(rel, i), nlen)) {
            rel = sie_rel_idx_set_value(rel, i, v, vlen);
	    return rel;
        }
    }

    i = rel->count;
    rel = sie_rel_idx_set_name(rel, i, name, nlen);
    rel = sie_rel_idx_set_value(rel, i, v, vlen);
    return rel;
}

int sie_rel_scan_value(sie_Relation *rel, const char *name, char *format, void *r)
{
    char *p = sie_rel_value(rel, name);
    if (p && sscanf(p, format, r) == 1)
	return 1;
    return 0;
}

int sie_rel_short_value(sie_Relation *rel, const char *name, short *i)
{
    return sie_rel_scan_value(rel, name, " %hd ", i);
}

int sie_rel_int_value(sie_Relation *rel, const char *name, int *i)
{
    return sie_rel_scan_value(rel, name, " %d ", i);
}

int sie_rel_float_value(sie_Relation *rel, const char *name, float *f)
{
    return sie_rel_scan_value(rel, name, " %f ", f);
}

int sie_rel_double_value(sie_Relation *rel, const char *name, double *d)
{
    return sie_rel_scan_value(rel, name, " %lf ", d);
}

void sie_rel_free(sie_Relation *rel)
{
    if (rel)
        free(rel);
}

void sie_rel_dump(sie_Relation *rel, const char *tag, FILE *fp)
{
    ssize_t i;

    for (i = 0; i < sie_rel_count(rel); i++) {
        fprintf(fp, "%s: %s => %s\n", tag,
            sie_rel_idx_name(rel, i), sie_rel_idx_value(rel, i));
    }
}

void sie_rel_diagnose(sie_Relation *rel, const char *name, FILE *fp)
{
    ssize_t i;
    int bite = 0;

    fprintf(fp, "%s (%p)\n", name, rel);
    if (!rel)
        return;
    fprintf(fp, "  count = %"APR_SSIZE_T_FMT"\n", rel->count);
    fprintf(fp, "  max = %"APR_SSIZE_T_FMT"\n", rel->max);
    fprintf(fp, "  size = %"APR_SSIZE_T_FMT"\n", rel->size);
    fprintf(fp, "  endp = %"APR_SSIZE_T_FMT"\n", rel->endp);
    if (rel->endp >= rel->size)
        bite = 1, fprintf(fp, "endp BROKEN\n");
    for (i = 0; i < rel->count; i++) {
        fprintf(fp, "[%"APR_SSIZE_T_FMT"]: (%*s) => (%*s)\n", i,
            (int)sie_rel_idx_name_size(rel, i),
            sie_rel_idx_name(rel, i),
            (int)sie_rel_idx_value_size(rel, i),
            sie_rel_idx_value(rel, i));
        fprintf(fp, "  params[%"APR_SSIZE_T_FMT"].name_off = %"APR_SSIZE_T_FMT"\n", i,
	    rel->params[i].name_off);
        fprintf(fp, "  params[%"APR_SSIZE_T_FMT"].name_len = %"APR_SSIZE_T_FMT"\n", i,
	    rel->params[i].name_len);
        fprintf(fp, "  params[%"APR_SSIZE_T_FMT"].value_off = %"APR_SSIZE_T_FMT"\n", i,
	    rel->params[i].value_off);
        fprintf(fp, "  params[%"APR_SSIZE_T_FMT"].value_len = %"APR_SSIZE_T_FMT"\n", i,
	    rel->params[i].value_len);
    }
    for (i = 0; i < rel->count; i++) {
        ssize_t off;
        off = rel->params[i].name_off;
        if (off >= rel->size)
            bite = 1, fprintf(fp, "  params[%"APR_SSIZE_T_FMT"].name_off BROKEN\n", i);
	off += rel->params[i].name_len;
        if (off >= rel->size)
            bite = 1, fprintf(fp, "  params[%"APR_SSIZE_T_FMT"].name_len BROKEN\n", i);
	if (off > rel->endp)
            bite = 1, fprintf(fp, "endp BROKEN\n");
        off = rel->params[i].value_off;
        if (off >= rel->size)
            bite = 1, fprintf(fp, "  params[%"APR_SSIZE_T_FMT"].value_off BROKEN\n", i);
	off += rel->params[i].value_len;
        if (off >= rel->size)
            bite = 1, fprintf(fp, "  params[%"APR_SSIZE_T_FMT"].value_len BROKEN\n", i);
	if (off > rel->endp)
            bite = 1, fprintf(fp, "endp BROKEN\n");
    }
    if (bite)
        abort();
}
