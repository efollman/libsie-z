/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#ifndef SIE_CONFIG_H
#define SIE_CONFIG_H

#ifdef __GNUC__
# define __gcc_attribute__(x) __attribute__(x)
# define SIE_HAS_C99_MACROS 1
#else
# define __gcc_attribute__(x)
#endif

#ifdef WIN32
# define SIE_BROKEN_UINT64
# define SIE_LITTLE_ENDIAN
#endif

#ifdef __cplusplus
#define EX_C extern "C"
#else
#define EX_C
#endif

#ifdef WIN32
# if defined(SIE_DECLARE_STATIC)
#  define SIE_DECLARE(type)          EX_C type __cdecl
#  define SIE_DECLARE_STD(type)      EX_C type __stdcall
#  define SIE_DECLARE_NONSTD(type)   EX_C type __cdecl
#  define SIE_DECLARE_DATA
# elif defined(SIE_DECLARE_EXPORT)
#  define SIE_DECLARE(type)          EX_C __declspec(dllexport) type __cdecl
#  define SIE_DECLARE_STD(type)      EX_C __declspec(dllexport) type __stdcall
#  define SIE_DECLARE_NONSTD(type)   EX_C __declspec(dllexport) type __cdecl
#  define SIE_DECLARE_DATA           __declspec(dllexport)
# else
#  define SIE_DECLARE(type)          EX_C __declspec(dllimport) type __cdecl
#  define SIE_DECLARE_STD(type)      EX_C __declspec(dllimport) type __stdcall
#  define SIE_DECLARE_NONSTD(type)   EX_C __declspec(dllimport) type __cdecl
#  define SIE_DECLARE_DATA           __declspec(dllimport)
# endif
#else
#  define SIE_DECLARE(type)          EX_C type
#  define SIE_DECLARE_STD(type)      EX_C type
#  define SIE_DECLARE_NONSTD(type)   EX_C type
#  define SIE_DECLARE_DATA
#endif

#include <stdarg.h>

#ifndef va_copy
# ifdef __va_copy
#  define va_copy(dest,src) __va_copy((dest), (src))
# else
#  define va_copy(dest,src) memcpy(&(dest), &(src), sizeof(va_list))
# endif
#endif

#include "sie_apr.h"

#ifdef WIN32
# define alloca _alloca
typedef apr_ssize_t ssize_t;
#endif

#include "sie_internal.h"

#endif
