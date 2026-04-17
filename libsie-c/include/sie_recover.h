/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_RECOVER_H
#define SIE_RECOVER_H

SIE_DECLARE(void) sie_file_recover(void *ctx_obj, const char *filename,
                                   size_t mod);

#endif

#endif
