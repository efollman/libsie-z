/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

# include "sie_config.h"

#ifdef SIE_LIBAPR_EMULATION

#include <stdio.h>
#include <string.h>
#include <errno.h>

void do_nothing(void)
{
}

apr_status_t apr_file_open(apr_file_t **newf, const char *fname,
    apr_int32_t flag, apr_fileperms_t perm, apr_pool_t *pool)
{
    if (!(*newf = fopen64(fname, flag ? "w" : "r")))
        return errno;
    return APR_SUCCESS;
}

apr_status_t apr_file_read(apr_file_t *thefile, void *buf, apr_size_t *nbytes)
{
    apr_size_t req_size = *nbytes;
    
    *nbytes = fread(buf, 1, *nbytes, thefile);
    return (*nbytes == req_size) ? APR_SUCCESS : errno;
}

apr_status_t apr_file_write(apr_file_t *thefile, void *buf, apr_size_t *nbytes)
{
    apr_size_t req_size = *nbytes;

    *nbytes = fwrite(buf, 1, *nbytes, thefile);
    return (*nbytes == req_size) ? APR_SUCCESS : errno;
}

apr_status_t apr_file_seek(apr_file_t *thefile, apr_seek_where_t where,
    apr_off_t *offset)
{
    if (fseeko64(thefile, *offset, where))
        return errno;
    if ((*offset = ftello64(thefile)) == (off64_t)-1)
        return errno;
    return APR_SUCCESS;
}

char *apr_strerror(apr_status_t statcode, char *buf, apr_size_t bufsize)
{
    /* KLUDGE - GNU-style */
    char *out;
    size_t outlen;
    out = strerror_r(statcode, buf, bufsize);
    outlen = strlen(out) + 1;
    if (bufsize < outlen)
        outlen = bufsize;
    memmove(buf, out, outlen);
    buf[bufsize - 1] = 0;
    return buf;

#if 0
    /* POSIX-style */
    strerror_r(statcode, buf, bufsize);
    return buf;
#endif
}

#endif /* SIE_APR_EMULATION */
