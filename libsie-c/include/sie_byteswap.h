/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

#elif defined(SIE_INCLUDE_BODIES)

#ifndef BYTESWAP_H
#define BYTESWAP_H

#ifndef WIN32
#include <netinet/in.h> /* For htonl() */
#else
#include <winsock2.h>
#endif

#ifdef LINUX
#include <byteswap.h>
#else
#ifdef WIN32
static unsigned short bswap_16(unsigned short a)
{
    unsigned short _tmp;                         
    ((char *)&_tmp)[0] = ((char *)&a)[1];   
    ((char *)&_tmp)[1] = ((char *)&a)[0];   
    return _tmp;
}
static unsigned long bswap_32(unsigned long a)
{
    unsigned long _tmp;                         
    ((char *)&_tmp)[0] = ((char *)&a)[3];   
    ((char *)&_tmp)[1] = ((char *)&a)[2];   
    ((char *)&_tmp)[2] = ((char *)&a)[1];   
    ((char *)&_tmp)[3] = ((char *)&a)[0];   
    return _tmp;
}
static unsigned __int64 bswap_64(unsigned __int64 a)
{
    unsigned __int64 _tmp;                         
    ((char *)&_tmp)[0] = ((char *)&a)[7];   
    ((char *)&_tmp)[1] = ((char *)&a)[6];   
    ((char *)&_tmp)[2] = ((char *)&a)[5];   
    ((char *)&_tmp)[3] = ((char *)&a)[4];   
    ((char *)&_tmp)[4] = ((char *)&a)[3];   
    ((char *)&_tmp)[5] = ((char *)&a)[2];   
    ((char *)&_tmp)[6] = ((char *)&a)[1];   
    ((char *)&_tmp)[7] = ((char *)&a)[0];   
    return _tmp;
}
#else /* Oh for God's sake! */
#define bswap_16(a)                             \
    ({  typeof(a) _tmp;                         \
        ((char *)&_tmp)[0] = ((char *)&a)[1];   \
        ((char *)&_tmp)[1] = ((char *)&a)[0];   \
        _tmp;  })
#define bswap_32(a)                             \
    ({  typeof(a) _tmp;                         \
        ((char *)&_tmp)[0] = ((char *)&a)[3];   \
        ((char *)&_tmp)[1] = ((char *)&a)[2];   \
        ((char *)&_tmp)[2] = ((char *)&a)[1];   \
        ((char *)&_tmp)[3] = ((char *)&a)[0];   \
        _tmp;  })
#define bswap_64(a)                             \
    ({  typeof(a) _tmp;                         \
        ((char *)&_tmp)[0] = ((char *)&a)[7];   \
        ((char *)&_tmp)[1] = ((char *)&a)[6];   \
        ((char *)&_tmp)[2] = ((char *)&a)[5];   \
        ((char *)&_tmp)[3] = ((char *)&a)[4];   \
        ((char *)&_tmp)[4] = ((char *)&a)[3];   \
        ((char *)&_tmp)[5] = ((char *)&a)[2];   \
        ((char *)&_tmp)[6] = ((char *)&a)[1];   \
        ((char *)&_tmp)[7] = ((char *)&a)[0];   \
        _tmp;  })
#endif
#endif

#define sie_byteswap_8(a)  a
#define sie_byteswap_16(a) bswap_16(*(sie_uint16 *)&(a))
#define sie_byteswap_32(a) bswap_32(*(sie_uint32 *)&(a))
#define sie_byteswap_64(a) bswap_64(*(sie_uint64 *)&(a))

#define sie_identity_8(a)  a
#define sie_identity_16(a) a
#define sie_identity_32(a) a
#define sie_identity_64(a) a

#define sie_hton32(a) htonl(a)
#define sie_hton16(a) htons(a)
#define sie_ntoh32(a) ntohl(a)
#define sie_ntoh16(a) ntohs(a)

SIE_DECLARE(sie_uint64) sie_hton64(sie_uint64 value);
SIE_DECLARE(sie_uint64) sie_ntoh64(sie_uint64 value);

#endif

#endif
