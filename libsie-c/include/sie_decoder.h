/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#if defined(SIE_FORWARD_TYPEDEFS)

typedef struct _sie_Decoder sie_Decoder;
typedef struct _sie_Decoder_Machine sie_Decoder_Machine;
typedef struct _sie_raw_register_t sie_raw_register_t;

#elif defined(SIE_INCLUDE_BODIES)

#ifndef SIE_DECODER_H
#define SIE_DECODER_H

/*
  Sucky formula for fixed integer reads:
  Type:
    Unsigned = 0
    Signed = 4
    Float = 6
  Size:
    8 = 0
    16 = 1
    32 = 2
    64 = 3
  Endian:
    Little-endian = 0
    Big-endian = 10

  Op = 1 + Type + Size + Endian
*/

typedef enum sie_Opcode {
    SIE_OP_CRASH = 0,

    SIE_OP_READ_U8LE  = 1,
    SIE_OP_READ_U16LE = 2,
    SIE_OP_READ_U32LE = 3,
    SIE_OP_READ_U64LE = 4,
    SIE_OP_READ_S8LE  = 5,
    SIE_OP_READ_S16LE = 6,
    SIE_OP_READ_S32LE = 7,
    SIE_OP_READ_S64LE = 8,
    SIE_OP_READ_F32LE = 9,
    SIE_OP_READ_F64LE = 10,
    SIE_OP_READ_U8BE  = 11,
    SIE_OP_READ_U16BE = 12,
    SIE_OP_READ_U32BE = 13,
    SIE_OP_READ_U64BE = 14,
    SIE_OP_READ_S8BE  = 15,
    SIE_OP_READ_S16BE = 16,
    SIE_OP_READ_S32BE = 17,
    SIE_OP_READ_S64BE = 18,
    SIE_OP_READ_F32BE = 19,
    SIE_OP_READ_F64BE = 20,

    SIE_OP_READ_RAW,
    SIE_OP_SEEK,

    SIE_OP_SAMPLE,

    SIE_OP_MR,

    SIE_OP_ADD,
    SIE_OP_SUB,
    SIE_OP_MUL,
    SIE_OP_DIV,
    SIE_OP_AND,
    SIE_OP_OR,
    SIE_OP_LSL,
    SIE_OP_LSR,
    SIE_OP_LNOT,

    SIE_OP_LT,
    SIE_OP_LE,
    SIE_OP_GT,
    SIE_OP_GE,
    SIE_OP_EQ,
    SIE_OP_NE,
    SIE_OP_LAND,
    SIE_OP_LOR,

    SIE_OP_CMP,

    SIE_OP_B,
    SIE_OP_BEQ,
    SIE_OP_BNE,
    SIE_OP_BL,
    SIE_OP_BLE,
    SIE_OP_BG,
    SIE_OP_BGE,

    SIE_OP_ASSERT,
} sie_Opcode;

#define SIE_NUM_OPS       SIE_OP_ASSERT

#define SIE_FLAG_EQUAL          0x1
#define SIE_FLAG_NOT_EQUAL      0x2
#define SIE_FLAG_LESS           0x4
#define SIE_FLAG_LESS_EQUAL     0x8
#define SIE_FLAG_GREATER       0x10
#define SIE_FLAG_GREATER_EQUAL 0x20

typedef double             sie_register_t;

struct _sie_Decoder {
    sie_Context_Object parent;
    size_t num_bytecodes;
    int *bytecode;
    size_t num_vs;
    int *vs;
    size_t num_registers;
    sie_register_t *initial_registers;
    sie_Relation *register_names;
};
SIE_CLASS_DECL(sie_Decoder);

struct _sie_raw_register_t {
    sie_vec_declare(vec, char);
};

struct _sie_Decoder_Machine {
    sie_Context_Object parent;
    sie_Decoder *decoder;
    sie_register_t *registers;
    sie_raw_register_t *raw_registers;
    int done;
    size_t pc;
    int flags;
    char *data;
    size_t data_size;
    size_t data_index;
    sie_Output *output;
};
SIE_CLASS_DECL(sie_Decoder_Machine);

SIE_DECLARE(sie_Decoder_Machine *) sie_decoder_machine_new(
    sie_Decoder *decoder);
SIE_DECLARE(void) sie_decoder_machine_init(sie_Decoder_Machine *self,
                                           sie_Decoder *decoder);
SIE_DECLARE(void) sie_decoder_machine_destroy(sie_Decoder_Machine *self);
SIE_DECLARE(void) sie_decoder_machine_prep(sie_Decoder_Machine *self,
                                           char *data, size_t data_size);
SIE_DECLARE(void) sie_decoder_machine_run(sie_Decoder_Machine *self);

SIE_DECLARE(sie_Decoder *) sie_decoder_new(void *ctx_obj, sie_XML *source);
SIE_DECLARE(char *) sie_decoder_disassemble(sie_Decoder *self);

SIE_DECLARE(void) sie_decoder_sigbuf(sie_Decoder *self,
                                     unsigned char **sigbuf, size_t *len);
SIE_DECLARE(sie_uint32) sie_decoder_signature(sie_Decoder *self);
SIE_DECLARE(int) sie_decoder_is_equal(sie_Decoder *self, sie_Decoder *other);

#endif

#endif
