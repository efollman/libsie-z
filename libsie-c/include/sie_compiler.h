/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

typedef struct _sie_Compiler sie_Compiler;

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_COMPILER_H
#define SIE_COMPILER_H

struct _sie_Compiler {
    sie_Context_Object parent;
    sie_vec_declare(bytecode, int);
    sie_vec_declare(label_locations, int);
    sie_vec_declare(label_fixups, int);
    sie_vec_declare(initial_registers, sie_register_t);

    sie_Relation *register_names;

    sie_vec_declare(temp_registers, char *);
    int next_temp_register;
};
SIE_CLASS_DECL(sie_Compiler);
#define SIE_COMPILER(p) SIE_SAFE_CAST(p, sie_Compiler)

SIE_DECLARE(sie_Compiler *) sie_compiler_new(void *ctx_obj);
SIE_DECLARE(void) sie_compiler_init(sie_Compiler *self, void *ctx_obj);
SIE_DECLARE(void) sie_compiler_destroy(sie_Compiler *self);

SIE_DECLARE(void) sie_compiler_compile_into_decoder(sie_Decoder *decoder,
                                                    sie_XML *source);

#endif

#endif
