/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#include "sie_config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "sie_xml.h"
#include "sie_types.h"
#include "sie_byteswap.h"
#include "sie_decoder.h"
#include "sie_compiler.h"

#define SIE_OP_FLAG_BRANCH 0x1

static struct sie_op {
    sie_Opcode op;
    char *name;
    size_t args;
    int flags;
} ops[] = {
    { SIE_OP_CRASH,      "crash",      0, 0 },

    { SIE_OP_READ_U8LE,  "read_u8le",  1, 0 },
    { SIE_OP_READ_U16LE, "read_u16le", 1, 0 },
    { SIE_OP_READ_U32LE, "read_u32le", 1, 0 },
    { SIE_OP_READ_U64LE, "read_u64le", 1, 0 },
    { SIE_OP_READ_S8LE,  "read_s8le",  1, 0 },
    { SIE_OP_READ_S16LE, "read_s16le", 1, 0 },
    { SIE_OP_READ_S32LE, "read_s32le", 1, 0 },
    { SIE_OP_READ_S64LE, "read_s64le", 1, 0 },
    { SIE_OP_READ_F32LE, "read_f32le", 1, 0 },
    { SIE_OP_READ_F64LE, "read_f64le", 1, 0 },
    { SIE_OP_READ_U8BE,  "read_u8be",  1, 0 },
    { SIE_OP_READ_U16BE, "read_u16be", 1, 0 },
    { SIE_OP_READ_U32BE, "read_u32be", 1, 0 },
    { SIE_OP_READ_U64BE, "read_u64be", 1, 0 },
    { SIE_OP_READ_S8BE,  "read_s8be",  1, 0 },
    { SIE_OP_READ_S16BE, "read_s16be", 1, 0 },
    { SIE_OP_READ_S32BE, "read_s32be", 1, 0 },
    { SIE_OP_READ_S64BE, "read_s64be", 1, 0 },
    { SIE_OP_READ_F32BE, "read_f32be", 1, 0 },
    { SIE_OP_READ_F64BE, "read_f64be", 1, 0 },

    { SIE_OP_READ_RAW,   "read_raw",   2, 0 },
    { SIE_OP_SEEK,       "seek",       2, 0 },

    { SIE_OP_SAMPLE,     "sample",     0, 0 },

    { SIE_OP_MR,         "mr",         2, 0 },

    { SIE_OP_ADD,        "add",        3, 0 },
    { SIE_OP_SUB,        "sub",        3, 0 },
    { SIE_OP_MUL,        "mul",        3, 0 },
    { SIE_OP_DIV,        "div",        3, 0 },
    { SIE_OP_AND,        "and",        3, 0 },
    { SIE_OP_OR,         "or",         3, 0 },
    { SIE_OP_LSL,        "lsl",        3, 0 },
    { SIE_OP_LSR,        "lsr",        3, 0 },
    { SIE_OP_LNOT,       "lnot",       2, 0 },

    { SIE_OP_LT,         "lt",         3, 0 },
    { SIE_OP_LE,         "le",         3, 0 },
    { SIE_OP_GT,         "gt",         3, 0 },
    { SIE_OP_GE,         "ge",         3, 0 },
    { SIE_OP_EQ,         "eq",         3, 0 },
    { SIE_OP_NE,         "ne",         3, 0 },
    { SIE_OP_LAND,       "land",       3, 0 },
    { SIE_OP_LOR,        "lor",        3, 0 },

    { SIE_OP_CMP,        "cmp",        2, 0 },

    { SIE_OP_B,          "b",          1, SIE_OP_FLAG_BRANCH },
    { SIE_OP_BEQ,        "beq",        1, SIE_OP_FLAG_BRANCH },
    { SIE_OP_BNE,        "bne",        1, SIE_OP_FLAG_BRANCH },
    { SIE_OP_BL,         "bl",         1, SIE_OP_FLAG_BRANCH },
    { SIE_OP_BLE,        "ble",        1, SIE_OP_FLAG_BRANCH },
    { SIE_OP_BG,         "bg",         1, SIE_OP_FLAG_BRANCH },
    { SIE_OP_BGE,        "bge",        1, SIE_OP_FLAG_BRANCH },

    { SIE_OP_ASSERT,     "assert",     2, 0 },
};

typedef union {
    unsigned char ch[8];
    sie_uint64 u64;
    double f64;
} sie_double_overlay;

/* Lovely KLUDGE for compilers that can't handle LL immediates */
static const unsigned char sie_raw_data_tag_bytes[] =
    { 0x7f, 0xf8, 0x53, 0x49, 0x45, 0x52, 0x41, 0x57 }; /* QNaN "SIERAW" */
static sie_double_overlay sie_raw_data_tag;
static int sie_raw_data_tag_initialized;

static void initialize_sie_raw_data_tag(void)
{
    int i, o = 0;
#ifdef SIE_BIG_ENDIAN   /* KLUDGE probably wrong for strange
                         * architectures */
    for (i = 0; i < 8; i++)
        sie_raw_data_tag.ch[o++] = sie_raw_data_tag_bytes[i];
#else
    for (i = 7; i >= 0; i--)
        sie_raw_data_tag.ch[o++] = sie_raw_data_tag_bytes[i];
#endif
    sie_raw_data_tag_initialized = 1;
}

static void sample(sie_Decoder_Machine *self)
{
    sie_Output *output = self->output;
    sie_Decoder *decoder = self->decoder;
    sie_register_t *regs = self->registers;
    sie_raw_register_t *raw_regs = self->raw_registers;
    size_t scan = output->num_scans;
    size_t v;

    for (v = 0; v < decoder->num_vs; v++) {
        sie_Output_V *vv = &output->v[v];
        sie_Output_V_Guts *vg = &output->v_guts[v];
        sie_double_overlay val;
        val.f64 = regs[decoder->vs[v]];

        if (vv->type == SIE_OUTPUT_NONE) {
            if (val.u64 == sie_raw_data_tag.u64) {
                vv->type = SIE_OUTPUT_RAW;
                vg->element_size = sizeof(*vv->raw);
            } else {
                vv->type = SIE_OUTPUT_FLOAT64;
                vg->element_size = sizeof(*vv->float64);
            }
        }

        if (vg->size >= vg->max_size)
            sie_output_grow(output, v);

        switch (vv->type) {
        case SIE_OUTPUT_FLOAT64:
            sie_assertf(val.u64 != sie_raw_data_tag.u64,
                        (self, "Output for v%"APR_SIZE_T_FMT" changed "
                         "from FLOAT64 to RAW.", v));
            vv->float64[scan] = val.f64;
            break;
        case SIE_OUTPUT_RAW:
            sie_assertf(val.u64 == sie_raw_data_tag.u64,
                        (self, "Output for v%"APR_SIZE_T_FMT" changed "
                         "from RAW to FLOAT64.", v));
            vv->raw[scan].size = sie_vec_octets(raw_regs[decoder->vs[v]].vec);
            vv->raw[scan].ptr = sie_malloc(self, vv->raw[scan].size);
            vv->raw[scan].claimed = 0;
            memcpy(vv->raw[scan].ptr, raw_regs[decoder->vs[v]].vec,
                   vv->raw[scan].size);
            break;
        default:
            sie_errorf((self, "Unknown type in v%"APR_SIZE_T_FMT" "
                        "in SAMPLE operation.", v));
            break;
        }

        vg->size++;
    }

    output->num_scans++;
}

#ifdef SIE_LITTLE_ENDIAN
#  define HOST_ENDIAN  LE
#  define OTHER_ENDIAN BE
#else /* SIE_BIG_ENDIAN */
#  define HOST_ENDIAN  BE
#  define OTHER_ENDIAN LE
#endif

#define READ_OP_LABEL(type_char, size, endian)                          \
    PP_CAT(SIE_OP_READ_, PP_CAT(type_char, PP_CAT(size, endian)))

#if 0
#define PRIMITIVE_READ_OP(type_char, size, type, endian, swap_fun)      \
    case READ_OP_LABEL(type_char, size, endian):                        \
    {                                                                   \
        type *datum_p = (type *)&data[data_index];                      \
        PP_CAT(sie_uint, size) swap;                                    \
        if (data_index + sizeof(*datum_p) > data_size) goto out;        \
        swap = PP_CAT(swap_fun, size)(*datum_p);                        \
        regs[insn[1]] = (type)*(type *)&swap;                           \
        data_index += sizeof(*datum_p);                                 \
        insn += 2;                                                      \
        break;                                                          \
    }
#endif

#define PRIMITIVE_READ_OP(type_char, size, type, endian, swap_fun)      \
    case READ_OP_LABEL(type_char, size, endian):                        \
    {                                                                   \
        union {                                                         \
            PP_CAT(sie_uint, size) u;                                   \
            type d;                                                     \
        } datum;                                                        \
        if (data_index + sizeof(type) > data_size)                      \
            goto out;                                                   \
        datum.u = PP_CAT(swap_fun, size)(*(PP_CAT(sie_uint, size) *)    \
                                         &data[data_index]);            \
        regs[insn[1]] = (sie_register_t)datum.d;                        \
        data_index += sizeof(type);                                     \
        insn += 2;                                                      \
        break;                                                          \
    }

#define PP_CAT(a, b) PP_PRIMITIVE_CAT(a, b)
#define PP_PRIMITIVE_CAT(a, b) a ## b

#define READ_OP(type_char, size, type)                                  \
    PRIMITIVE_READ_OP(type_char, size, type, HOST_ENDIAN, sie_identity_); \
    PRIMITIVE_READ_OP(type_char, size, type, OTHER_ENDIAN, sie_byteswap_);

#define MATH_OP(name, op)                               \
    case SIE_OP_ ## name:                               \
    {                                                   \
        regs[insn[1]] = regs[insn[2]] op regs[insn[3]]; \
        insn += 4;                                      \
        break;                                          \
    }

#define UINT64_MATH_OP(name, op)                                        \
    case SIE_OP_ ## name:                                               \
    {                                                                   \
        regs[insn[1]] = (sie_register_t)                                \
            ((sie_uint64)regs[insn[2]] op (sie_uint64)regs[insn[3]]);   \
        insn += 4;                                                      \
        break;                                                          \
    }

#define CONDITIONAL_BRANCH_OP(name, flag)       \
    case SIE_OP_ ## name:                       \
    {                                           \
        if (flags & flag) insn += insn[1];      \
        insn += 2;                              \
        break;                                  \
    }

static char *get_reg_name(sie_Decoder *decoder, int reg)
{
    char *name = NULL;
    int i;
    for (i = 0; i < sie_rel_count(decoder->register_names); i++) {
        int cur_reg;
        if (sie_rel_int_value(
                decoder->register_names,
                sie_rel_idx_name(decoder->register_names, i),
                &cur_reg) && cur_reg == reg)
            name = sie_rel_idx_name(decoder->register_names, i);
    }
    sie_assert(name, decoder);
    return name;
}

void sie_decoder_machine_run(sie_Decoder_Machine *self)
{
    sie_Decoder *decoder = self->decoder;
    char *data = self->data;
    size_t data_size = self->data_size;
    size_t data_index = self->data_index;
    sie_register_t *regs = self->registers;
    sie_raw_register_t *raw_regs = self->raw_registers;
    int *insn = decoder->bytecode + self->pc;
    int flags = self->flags;
    int *first_insn = decoder->bytecode;
    int *last_insn = first_insn + decoder->num_bytecodes;

    while (insn >= first_insn && insn < last_insn) {
#if 0
        int i;
        printf("\npc: %"APR_SIZE_T_FMT"\n", insn - first_insn);
        printf("op: %d", insn[0]);
        for (i = 0; i < sizeof(ops)/sizeof(*ops); i++) {
            if (ops[i].op == insn[0]) {
                int j;
                printf("\t(%s ", ops[i].name);
                for (j = 0; j < ops[i].args; j++) {
                    if (j) printf(", ");
                    if (ops[i].flags == SIE_OP_FLAG_BRANCH) {
                        printf("%d", insn[1 + j]);
                    } else {
                        char *name = get_reg_name(decoder, insn[1 + j]);
                        if (strncmp(name, "0X", 2) == 0)
                            printf("%g", regs[insn[1 + j]]);
                        else
                            printf("%s", name);
                    }
                }
                printf(")");
                break;
            }
        }
        printf("\n");
        printf("flags: ");
        if (flags & SIE_FLAG_EQUAL) printf("SIE_FLAG_EQUAL ");
        if (flags & SIE_FLAG_NOT_EQUAL) printf("FLAG_NOT_EQUAL ");
        if (flags & SIE_FLAG_LESS) printf("SIE_FLAG_LESS ");
        if (flags & SIE_FLAG_LESS_EQUAL) printf("FLAG_LESS_EQUAL ");
        if (flags & SIE_FLAG_GREATER) printf("SIE_FLAG_GREATER ");
        if (flags & SIE_FLAG_GREATER_EQUAL) printf("FLAG_GREATER_EQUAL ");
        printf("\n");
        for (i = 0; i < self->decoder->num_registers; i++) {
            char *name = get_reg_name(decoder, i);
            if (strncmp(name, "0X", 2) != 0)
                printf("%s: %g\n", name, regs[i]);
        }
#endif
        switch ((sie_Opcode)insn[0]) {
        case SIE_OP_CRASH:
            sie_errorf((self, "SIE_OP_CRASH executed"));
            break;

        READ_OP(U, 8,  sie_uint8);
        READ_OP(U, 16, sie_uint16);
        READ_OP(U, 32, sie_uint32);
        READ_OP(U, 64, sie_uint64);
        READ_OP(S, 8,  sie_int8);
        READ_OP(S, 16, sie_int16);
        READ_OP(S, 32, sie_int32);
        READ_OP(S, 64, sie_int64);
        READ_OP(F, 32, sie_float32);
        READ_OP(F, 64, sie_float64);

        case SIE_OP_READ_RAW: {
            size_t count = (size_t)regs[insn[2]];
            if (data_index + count < data_index)  /* rollover */
                goto out;
            if (data_index + count > data_size)   /* not enough data */
                goto out;
            regs[insn[1]] = sie_raw_data_tag.f64;
            sie_vec_clear(raw_regs[insn[1]].vec);
            sie_vec_append(raw_regs[insn[1]].vec, &data[data_index], count);
            data_index += count;
            insn += 3;
            break;
        }

        case SIE_OP_SEEK:
            switch ((int)regs[insn[2]]) {
            case 0: /* SEEK_SET */
                data_index = (size_t)regs[insn[1]];
                break;
            case 1: /* SEEK_CUR */
                data_index += (size_t)regs[insn[1]];
                break;
            case 2: /* SEEK_END */
                data_index = data_size + (size_t)regs[insn[1]];
                break;
            default:
                sie_errorf((self, "Unknown seek target %d",
                            (int)regs[insn[2]]));
            }
            insn += 3;
            break;

        case SIE_OP_SAMPLE:
            sample(self);
            insn += 1;
            break;

        case SIE_OP_MR:
            regs[insn[1]] = regs[insn[2]];
            insn += 3;
            break;

        MATH_OP(ADD, +);
        MATH_OP(SUB, -);
        MATH_OP(MUL, *);
        MATH_OP(DIV, /);
        UINT64_MATH_OP(AND, &);
        UINT64_MATH_OP(OR, |);
        UINT64_MATH_OP(LSL, <<);
        UINT64_MATH_OP(LSR, >>);
        
        case SIE_OP_LNOT:
            regs[insn[1]] = !regs[insn[2]];
            insn += 3;
            break;

        MATH_OP(LT, <);
        MATH_OP(LE, <=);
        MATH_OP(GT, >);
        MATH_OP(GE, >=);
        MATH_OP(EQ, ==);
        MATH_OP(NE, !=);
        MATH_OP(LAND, &&);
        MATH_OP(LOR, ||);

        case SIE_OP_CMP:
            if (regs[insn[1]] == regs[insn[2]])
                flags = (SIE_FLAG_EQUAL |
                         SIE_FLAG_LESS_EQUAL |
                         SIE_FLAG_GREATER_EQUAL);
            else if (regs[insn[1]] < regs[insn[2]])
                flags = (SIE_FLAG_LESS |
                         SIE_FLAG_LESS_EQUAL |
                         SIE_FLAG_NOT_EQUAL);
            else
                flags = (SIE_FLAG_GREATER |
                         SIE_FLAG_GREATER_EQUAL |
                         SIE_FLAG_NOT_EQUAL);
            insn += 3;
            break;

        case SIE_OP_B:
            insn += insn[1];
            insn += 2;
            break;

        CONDITIONAL_BRANCH_OP(BEQ, SIE_FLAG_EQUAL);
        CONDITIONAL_BRANCH_OP(BNE, SIE_FLAG_NOT_EQUAL);
        CONDITIONAL_BRANCH_OP(BL,  SIE_FLAG_LESS);
        CONDITIONAL_BRANCH_OP(BLE, SIE_FLAG_LESS_EQUAL);
        CONDITIONAL_BRANCH_OP(BG,  SIE_FLAG_GREATER);
        CONDITIONAL_BRANCH_OP(BGE, SIE_FLAG_GREATER_EQUAL);

        case SIE_OP_ASSERT:
            sie_assertf(regs[insn[1]] == regs[insn[2]],
                        (self, "Decoder assertion %f == %f failed.",
                         regs[insn[1]], regs[insn[2]]));
            insn += 3;
        }
    }

out:
    self->done = 1;
    self->pc = insn - decoder->bytecode;
    self->flags = flags;
    self->data_index = data_index;
}

static struct sie_op *get_op(sie_Decoder *self, sie_Opcode opcode)
{
    size_t op;
    for (op = 0; op < sizeof(ops)/sizeof(*ops); op++) {
        if (ops[op].op == opcode)
            return &ops[op];
    }
    sie_assert(0, self);
    return NULL;
}

char *sie_decoder_disassemble(sie_Decoder *self)
{
    size_t pc = 0;
    size_t next_pc;
    sie_vec(branch_targets, size_t, 0);
    sie_vec(ret, char, 0);
    char *retval;
    size_t i;

    for (i = 0; i < self->num_registers; i++) {
        char *name = get_reg_name(self, (int)i);
        if (strncmp(name, "0X", 2) == 0)
            sie_vec_strcatf(self, &ret, ";;; register %"APR_SIZE_T_FMT": "
                            "constant %g (%s)\n",
                            i, self->initial_registers[i], name);
        else
            sie_vec_strcatf(self, &ret, ";;; register %"APR_SIZE_T_FMT": %s\n", 
                            i, name);
    }

    while (pc < self->num_bytecodes) {
        struct sie_op *op = get_op(self, self->bytecode[pc]);
        next_pc = pc + 1 + op->args;
        if (op->flags & SIE_OP_FLAG_BRANCH) {
            size_t target = next_pc + self->bytecode[pc + 1];
            sie_vec_push_back(branch_targets, target);
        }
        pc = next_pc;
    }

    pc = 0;
    while (pc < self->num_bytecodes) {
        struct sie_op *op = get_op(self, self->bytecode[pc]);
        next_pc = pc + 1 + op->args;
        sie_vec_strcatf(self, &ret, "%"APR_SIZE_T_FMT"\t", pc);
        for (i = 0; i < sie_vec_size(branch_targets); i++) {
            if (branch_targets[i] == pc)
                sie_vec_strcatf(self, &ret, "l%"APR_SIZE_T_FMT, i);
        }
        sie_vec_strcatf(self, &ret, "\t%s", op->name);
        if (op->flags & SIE_OP_FLAG_BRANCH) {
            for (i = 0; i < sie_vec_size(branch_targets); i++) {
                if (branch_targets[i] == next_pc + self->bytecode[pc + 1])
                    sie_vec_strcatf(self, &ret, "  l%"APR_SIZE_T_FMT, i);
            }
        } else {
            for (i = 0; i < op->args; i++) {
                int our_reg = self->bytecode[pc + 1 + i];
                char *name = NULL;
                int j;
                for (j = 0; j < sie_rel_count(self->register_names); j++) {
                    int reg;
                    if (sie_rel_int_value(
                            self->register_names,
                            sie_rel_idx_name(self->register_names, j),
                            &reg) && reg == our_reg)
                        name = sie_rel_idx_name(self->register_names, j);
                }
                sie_assert(name, self);
                if (strncmp(name, "0X", 2) == 0) {
                    sie_vec_strcatf(self, &ret, "%s%g",
                                    i ? ", " : "  ",
                                    self->initial_registers[our_reg]);
                } else {
                    sie_vec_strcatf(self, &ret, "%s%s", i ? ", " : "  ", name);
                }
            }
        }
        sie_vec_strcatf(self, &ret, "\n");
        pc = next_pc;
    }
    for (i = 0; i < sie_vec_size(branch_targets); i++) {
        if (branch_targets[i] == pc)
            sie_vec_strcatf(self, &ret, "\tl%"APR_SIZE_T_FMT"\n", i);
    }

    retval = sie_strdup(self, ret);
    sie_vec_free(ret);

    return retval;
}

SIE_CONTEXT_OBJECT_NEW_FN(sie_decoder_machine_new, sie_Decoder_Machine,
                          self, decoder, (sie_Decoder *decoder),
                          sie_decoder_machine_init(self, decoder));

void sie_decoder_machine_init(sie_Decoder_Machine *self, sie_Decoder *decoder)
{
    sie_context_object_init(SIE_CONTEXT_OBJECT(self), decoder);
    if (!sie_raw_data_tag_initialized)
        initialize_sie_raw_data_tag();

    self->decoder = sie_retain(decoder);
    self->registers =
        sie_calloc(decoder, decoder->num_registers * sizeof(*self->registers));
    self->raw_registers =
        sie_calloc(decoder, decoder->num_registers * sizeof(*self->raw_registers));
    self->output = sie_output_new(self, decoder->num_vs);
}

void sie_decoder_machine_destroy(sie_Decoder_Machine *self)
{
    size_t i;
    sie_release(self->output);
    for (i = 0; i < self->decoder->num_registers; i++)
        sie_vec_free(self->raw_registers[i].vec);
    free(self->raw_registers);
    free(self->registers);
    sie_release(self->decoder);
    sie_context_object_destroy(SIE_CONTEXT_OBJECT(self));
}

SIE_CLASS(sie_Decoder_Machine, sie_Context_Object,
          SIE_MDEF(sie_destroy, sie_decoder_machine_destroy));

void sie_decoder_machine_prep(sie_Decoder_Machine *self,
                              char *data, size_t data_size)
{
    size_t i;
    memcpy(self->registers, self->decoder->initial_registers,
           self->decoder->num_registers * sizeof(sie_register_t));
    for (i = 0; i < self->decoder->num_registers; i++) {
        sie_vec_clear(self->raw_registers[i].vec);
    }
    self->done = 0;
    self->pc = 0;
    self->flags = 0;
    self->data = data;
    self->data_size = data_size;
    self->data_index = 0;
    self->output = sie_output_maybe_reuse(self->output);
}

void sie_decoder_init(sie_Decoder *self, void *ctx_obj, sie_XML *source)
{
    sie_context_object_init(SIE_CONTEXT_OBJECT(self), ctx_obj);
    sie_compiler_compile_into_decoder(self, source);
}

SIE_CONTEXT_OBJECT_NEW_FN(sie_decoder_new, sie_Decoder, self, ctx_obj,
                          (void *ctx_obj, sie_XML *source),
                          sie_decoder_init(self, ctx_obj, source));

void sie_decoder_destroy(sie_Decoder *self)
{
    sie_rel_free(self->register_names);
    free(self->initial_registers);
    free(self->vs);
    free(self->bytecode);
    sie_context_object_destroy(SIE_CONTEXT_OBJECT(self));
}

void sie_decoder_sigbuf(sie_Decoder *self,
                        unsigned char **sigbuf, size_t *len)
{
    size_t bytecode_len   = self->num_bytecodes * sizeof(*self->bytecode);
    size_t vs_len         = self->num_vs * sizeof(*self->vs);
    size_t regs_len       = self->num_registers * sizeof(*self->vs);

    *len = bytecode_len + vs_len + regs_len;
    *sigbuf = sie_malloc(self, *len);
    memcpy(*sigbuf, self->bytecode, bytecode_len);
    memcpy(*sigbuf + bytecode_len, self->vs, vs_len);
    memcpy(*sigbuf + bytecode_len + vs_len, self->initial_registers, regs_len);
}

sie_uint32 sie_decoder_signature(sie_Decoder *self)
{
    size_t sigbuf_len;
    unsigned char *sigbuf;
    sie_uint32 sig;
    sie_decoder_sigbuf(self, &sigbuf, &sigbuf_len);
    sig = sie_crc(sigbuf, sigbuf_len);
    free(sigbuf);
    return sig;
}

int sie_decoder_is_equal(sie_Decoder *self, sie_Decoder *other)
{
    size_t len, len_other;
    unsigned char *buf, *buf_other;
    int equal = 0;
    sie_decoder_sigbuf(self, &buf, &len);
    sie_decoder_sigbuf(other, &buf_other, &len_other);
    if (len == len_other)
        equal = !memcmp(buf, buf_other, len);
    free(buf);
    free(buf_other);
    return equal;
}

SIE_CLASS(sie_Decoder, sie_Context_Object,
          SIE_MDEF(sie_destroy, sie_decoder_destroy));

