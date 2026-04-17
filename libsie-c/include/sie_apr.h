/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#ifndef SIE_APR_H
#define SIE_APR_H

/* Must be included from sie_config.h */

#ifndef SIE_LIBAPR_EMULATION

#include <apr.h>
#include <apr_general.h>
#include <apr_pools.h>
#include <apr_strings.h>
#include <apr_file_io.h>

typedef signed char   sie_int8;
typedef apr_int16_t   sie_int16;
typedef apr_int32_t   sie_int32;
typedef apr_int64_t   sie_int64;
typedef unsigned char sie_uint8;
typedef apr_uint16_t  sie_uint16;
typedef apr_uint32_t  sie_uint32;
#ifdef SIE_BROKEN_UINT64
typedef apr_int64_t   sie_uint64;   /* KLUDGE */
#else
typedef apr_uint64_t  sie_uint64;
#endif
typedef float         sie_float32;
typedef double        sie_float64;

#else /* SIE_LIBAPR_EMULATION */

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

/* apr_errno.h */
#define APR_SUCCESS 0
/* KLUDGE - rest are just from errno.h, but this doesn't affect libsie. */

typedef int apr_status_t;
typedef void *apr_pool_t;

typedef size_t apr_size_t;
typedef ssize_t apr_ssize_t;
typedef off64_t apr_off_t;
typedef FILE apr_file_t;
typedef off64_t apr_seek_where_t;
typedef int apr_int32_t; /* KLUDGE */
typedef unsigned int apr_fileperms_t;

/* KLUDGE generate this stuff like apr.h.in */
typedef signed char   sie_int8;
typedef signed short  sie_int16;
typedef signed int    sie_int32;
typedef signed long long sie_int64;
typedef unsigned char sie_uint8;
typedef unsigned short sie_uint16;
typedef unsigned int  sie_uint32;
typedef unsigned long long sie_uint64;
typedef float         sie_float32;
typedef double        sie_float64;

#define APR_SIZE_T_FMT "u"
#define APR_SSIZE_T_FMT "d"
#define APR_OFF_T_FMT "lld"
#define APR_UINT64_T_FMT "llu"

/* apr_file_io.h */
#define APR_SET SEEK_SET
#define APR_CUR SEEK_CUR
#define APR_END SEEK_END

/* KLUDGES */
#define APR_READ 0
#define APR_BINARY 0
#define APR_BUFFERED 0
#define APR_OS_DEFAULT 0
#define APR_WRITE 1
#define APR_CREATE 1
#define APR_TRUNCATE 1

#define apr_initialize() (APR_SUCCESS)
#define apr_terminate do_nothing
#define apr_pool_create(a1, a2) (APR_SUCCESS)
#define apr_pool_destroy(pool)
#define apr_file_close(file) fclose(file)
#define apr_vsnprintf vsnprintf

apr_status_t apr_file_open(apr_file_t **newf, const char *fname,
                           apr_int32_t flag, apr_fileperms_t perm,
                           apr_pool_t *pool);
apr_status_t apr_file_close(apr_file_t *file);
apr_status_t apr_file_read(apr_file_t *thefile, void *buf,
                           apr_size_t *nbytes);
apr_status_t apr_file_write(apr_file_t *thefile, void *buf,
                            apr_size_t *nbytes);
apr_status_t apr_file_seek(apr_file_t *thefile,
                           apr_seek_where_t where,
                           apr_off_t *offset);
char *apr_strerror(apr_status_t statcode, char *buf,
                   apr_size_t bufsize);
void do_nothing(void);

#endif /* SIE_LIBAPR_EMULATION */

#endif /* SIE_APR_H */
