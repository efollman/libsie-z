/*
 * Copyright (C) 2005-2015  HBM Inc., SoMat Products
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU LGPL.  See the
 * COPYING and include/sie.h.in files for more information.
 */

#define SIE_VEC_CONTEXT_OBJECT compiler

#include "sie_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "sie_vec.h"
#include "sie_compiler.h"
#include "sie_parser.h"
#include "sie_utils.h"

static int next_label(sie_Compiler *compiler)
{
    int next = (int)sie_vec_size(compiler->label_locations);
    sie_vec_push_back(compiler->label_locations, -1);
    return next;
}

static int save_temp_registers(sie_Compiler *compiler)
{
    return compiler->next_temp_register;
}

static void restore_temp_registers(sie_Compiler *compiler, int saved)
{
    compiler->next_temp_register = saved;
}

static char *next_temp_register(sie_Compiler *compiler)
{
    size_t next = compiler->next_temp_register++;
    char *retval;

    sie_assert(next <= sie_vec_size(compiler->temp_registers), compiler);
    if (next == sie_vec_size(compiler->temp_registers)) {
        retval = sie_calloc(compiler, 12);  /* KLUDGE! */
        sprintf(retval, "r%"APR_SIZE_T_FMT, next);
        sie_vec_push_back(compiler->temp_registers, retval);
    } else {
        retval = compiler->temp_registers[next];
    }

    return retval;
}

static void dump_register_bytes(sie_register_t reg, char *buf)
{
    union {
        sie_register_t r;
        unsigned char c[sizeof(sie_register_t)];
    } u;
    size_t i;
    u.r = reg;
    strcpy(buf, "0X");
    for (i = 0; i < sizeof(reg); i++)
        sprintf(buf + 2 + i * 2, "%02X", u.c[i]);
}

typedef enum sie_value_type sie_value_type;
enum sie_value_type {
    SIE_NAME = 1,
    SIE_NUMBER = 2,
    SIE_EXPRESSION = 4,
};
#define SIE_NOT_NAME (SIE_NUMBER | SIE_EXPRESSION)

static sie_value_type value_type(void *ctx_obj, const char *name)
{
    char c1;
    
    if (!name)
        return 0;

    c1 = toupper(name[0]);

    if (c1 == '{')
        return SIE_EXPRESSION;
    if ((c1 >= 'A' && c1 <= 'Z') || c1 == '_')
        return SIE_NAME;
    if ((c1 >= '0' && c1 <= '9') || c1 == '.' || c1 == '-')
        return SIE_NUMBER;

    sie_errorf((ctx_obj, "unknown value type"));
    return 0; /* Can't reach, yet MSVC complains anyway. */
}

static void check_type(void *ctx_obj, const char *name, sie_value_type types)
{
    /* KLUDGE FIXME */
    sie_assert(types & value_type(ctx_obj, name), ctx_obj);
}

static sie_register_t get_number_value(void *ctx_obj, const char *name)
{
    sie_register_t value;
    int consumed;
    int result = sie_scan_number(name, &value, &consumed);
    sie_assert(result, ctx_obj);
    return value;
}

static const char *compile_expr_node(sie_Compiler *compiler,
                                     const char *return_reg,
                                     sie_XML *node);

static int compile_text_expr_to_register(sie_Compiler *compiler,
                                         const char *return_reg,
                                         const char *text);

static int resolve_register(sie_Compiler *compiler, const char *name)
{
    int reg;
    const char *name_used = NULL;
    sie_register_t initial_value = 0.0;

    sie_assert(name, compiler);

    switch (value_type(compiler, name)) {
    case SIE_NAME:
        name_used = name;
        break;
    case SIE_NUMBER: {
        char buf[sizeof(sie_register_t) * 2 + 3];
        initial_value = get_number_value(compiler, name);
        dump_register_bytes(initial_value, buf);
        name_used = buf;
        break;
    }
    case SIE_EXPRESSION: 
        return compile_text_expr_to_register(compiler, NULL, name);
        break;
    }

    if (sie_rel_int_value(compiler->register_names, name_used, &reg)) {
        sie_assert(reg >= 0, compiler);
        return reg;
    } else {
        reg = (int)sie_vec_size(compiler->initial_registers);
        sie_vec_push_back(compiler->initial_registers, initial_value);
        compiler->register_names =
            sie_rel_set_valuef(compiler->register_names, name_used, "%d", reg);
        return reg;
    }
}

static void emit_label(sie_Compiler *compiler, size_t label)
{
    sie_assert(label < sie_vec_size(compiler->label_locations), compiler);
    sie_assert(compiler->label_locations[label] == -1, compiler);

    compiler->label_locations[label] = (int)sie_vec_size(compiler->bytecode);
}

static void emit_0arg(sie_Compiler *compiler, sie_Opcode op)
{
    sie_assert(op > 0 && op <= SIE_NUM_OPS, compiler);

    sie_vec_push_back(compiler->bytecode, op);
}

static void emit_1arg(sie_Compiler *compiler, sie_Opcode op,
                      const char *a1)
{
    int r1 = resolve_register(compiler, a1);

    sie_assert(op > 0 && op <= SIE_NUM_OPS, compiler);

    sie_vec_push_back(compiler->bytecode, op);
    sie_vec_push_back(compiler->bytecode, r1);
}

static void emit_2arg(sie_Compiler *compiler, sie_Opcode op,
                      const char *a1, const char *a2)
{
    int r1 = resolve_register(compiler, a1);
    int r2 = resolve_register(compiler, a2);

    sie_assert(op > 0 && op <= SIE_NUM_OPS, compiler);

    sie_vec_push_back(compiler->bytecode, op);
    sie_vec_push_back(compiler->bytecode, r1);
    sie_vec_push_back(compiler->bytecode, r2);
}

static void emit_3arg(sie_Compiler *compiler, sie_Opcode op,
                      const char *a1, const char *a2, const char *a3)
{
    int r1 = resolve_register(compiler, a1);
    int r2 = resolve_register(compiler, a2);
    int r3 = resolve_register(compiler, a3);

    sie_assert(op > 0 && op <= SIE_NUM_OPS, compiler);

    sie_vec_push_back(compiler->bytecode, op);
    sie_vec_push_back(compiler->bytecode, r1);
    sie_vec_push_back(compiler->bytecode, r2);
    sie_vec_push_back(compiler->bytecode, r3);
}

static void emit_mr(sie_Compiler *compiler, sie_Opcode op,
                    const char *dest, const char *src)
{
    /* This is its own function so it can support eliminating the MR
       instruction if src is an expression. */
    
    int r1 = resolve_register(compiler, dest);
    int r2;

    if (value_type(compiler, src) == SIE_EXPRESSION) {
        compile_text_expr_to_register(compiler, dest, src);
        return;
    } else {
        r2 = resolve_register(compiler, src);
    }
    
    sie_assert(op == SIE_OP_MR, compiler);

    sie_vec_push_back(compiler->bytecode, op);
    sie_vec_push_back(compiler->bytecode, r1);
    sie_vec_push_back(compiler->bytecode, r2);
}

static void emit_branch(sie_Compiler *compiler, sie_Opcode op, int label)
{
    int fixup_loc;

    sie_assert(op > 0 && op <= SIE_NUM_OPS, compiler);
    
    sie_vec_push_back(compiler->bytecode, op);
    fixup_loc = (int)sie_vec_size(compiler->bytecode);
    sie_vec_push_back(compiler->bytecode, 0); /* placeholder */
    
    sie_vec_push_back(compiler->label_fixups, label);
    sie_vec_push_back(compiler->label_fixups, fixup_loc);
}

static int compile_text_expr_to_register(sie_Compiler *compiler,
                                         const char *return_reg,
                                         const char *text)
{
    size_t mark = sie_cleanup_mark(compiler);
    sie_XML *tree = sie_autorelease(parse_text_expression(compiler, text));
    int reg;
    if (return_reg && tree->value.element.name == sie_literal(compiler, v)) {
        if (strcmp(sie_xml_get_attribute(tree, "n"), return_reg))
            emit_mr(compiler, SIE_OP_MR,
                    return_reg, sie_xml_get_attribute(tree, "n"));
    }
    reg = resolve_register(compiler,
                           compile_expr_node(compiler, return_reg, tree));
    sie_cleanup_pop_mark(compiler, mark);
    return reg;
}

static void fixup_labels(sie_Compiler *compiler)
{
    while (sie_vec_size(compiler->label_fixups) > 0) {
        int fixup_loc, label;

        fixup_loc = *sie_vec_back(compiler->label_fixups);
        sie_vec_pop_back(compiler->label_fixups);
        label = *sie_vec_back(compiler->label_fixups);
        sie_vec_pop_back(compiler->label_fixups);

        sie_assert(compiler->label_locations[label] >= 0, compiler);

        compiler->bytecode[fixup_loc] =
            compiler->label_locations[label] - fixup_loc - 1;
    }
}

SIE_CONTEXT_OBJECT_NEW_FN(sie_compiler_new, sie_Compiler, self, ctx_obj,
                          (void *ctx_obj), sie_compiler_init(self, ctx_obj));

void sie_compiler_init(sie_Compiler *self, void *ctx_obj)
{
    sie_context_object_init(SIE_CONTEXT_OBJECT(self), ctx_obj);

    _sie_vec_init(self->bytecode, 0);
    _sie_vec_init(self->label_locations, 0);
    _sie_vec_init(self->label_fixups, 0);
    _sie_vec_init(self->initial_registers, 0);
    
    self->register_names = sie_rel_new(0, 1024);

    _sie_vec_init(self->temp_registers, 0);
    self->next_temp_register = 0;
}

void sie_compiler_destroy(sie_Compiler *self)
{
    char **reg_name_p;
    sie_vec_forall(self->temp_registers, reg_name_p) {
        free(*reg_name_p);
    }
    sie_vec_free(self->temp_registers);
    sie_vec_free(self->initial_registers);
    sie_vec_free(self->label_fixups);
    sie_vec_free(self->label_locations);
    sie_vec_free(self->bytecode);
    
    sie_rel_free(self->register_names);

    sie_context_object_destroy(SIE_CONTEXT_OBJECT(self));
}

SIE_CLASS(sie_Compiler, sie_Context_Object,
          SIE_MDEF(sie_destroy, sie_compiler_destroy));

static const char *get_optional_arg(sie_XML *node, const char *name,
                                    sie_value_type types)
{
    const char *retval = sie_xml_get_attribute(node, name);
    if (retval) check_type(node, retval, types);
    return retval;
}

static const char *get_required_arg(sie_XML *node, const char *name,
                                    sie_value_type types)
{
    const char *retval = get_optional_arg(node, name, types);
    sie_assert(retval, node);
    return retval;
}

static void compile_node(sie_Compiler *compiler, sie_XML *node);

static void compile_children(sie_Compiler *compiler, sie_XML *node)
{
    sie_XML *cur = node->child;
    while (cur) {
        compile_node(compiler, cur);
        cur = sie_xml_walk_next(cur, node, SIE_XML_NO_DESCEND);
    }
}

static void compile_decoder(sie_Compiler *compiler, sie_XML *node)
{
    compile_children(compiler, node);
}

static void compile_set(sie_Compiler *compiler, sie_XML *node)
{
    const char *var   = get_required_arg(node, "var", SIE_NAME);
    const char *value = get_required_arg(node, "value", SIE_NOT_NAME);

    sie_assert(var && value, compiler);

    emit_mr(compiler, SIE_OP_MR, var, value);
}

static void compile_loop(sie_Compiler *compiler, sie_XML *node)
{
    const char *var         = get_optional_arg(node, "var", SIE_NAME);
    const char *start       = get_optional_arg(node, "start", SIE_NOT_NAME);
    const char *end         = get_optional_arg(node, "end", SIE_NOT_NAME);
    const char *increment_s =
        get_optional_arg(node, "increment", SIE_NOT_NAME);
    const char *increment   = increment_s ? increment_s : "1";
    
    int const_increment_p = (value_type(compiler, increment) == SIE_NUMBER);
    sie_register_t const_increment = 0;
    int zero_increment_p = 0;

    int loop_end        = next_label(compiler);
    int loop_up_start   = next_label(compiler);
    int loop_down_start = next_label(compiler);

    if (!var) {
        /* Simple infinite loop */
        sie_assert(!(start || end || increment_s), compiler);
        emit_label(compiler, loop_up_start);
        compile_children(compiler, node);
        emit_branch(compiler, SIE_OP_B, loop_up_start);
        return;
    }

    /* Complex loop */

    if (start)
        emit_mr(compiler, SIE_OP_MR, var, start);

    if (const_increment_p) {
        const_increment = get_number_value(compiler, increment);
        if (const_increment == 0)
            zero_increment_p = 1;
    } else {
        emit_2arg(compiler, SIE_OP_CMP, increment, "0");
        emit_branch(compiler, SIE_OP_BL, loop_down_start);
    }

    if (!const_increment_p || const_increment >= 0) {
        emit_label(compiler, loop_up_start);
        if (end) {
            emit_2arg(compiler, SIE_OP_CMP, var, end);
            emit_branch(compiler, SIE_OP_BGE, loop_end);
        }
        compile_children(compiler, node);
        if (!zero_increment_p)
            emit_3arg(compiler, SIE_OP_ADD, var, var, increment);
        emit_branch(compiler, SIE_OP_B, loop_up_start);
    }

    if (!const_increment_p || const_increment < 0) {
        emit_label(compiler, loop_down_start);
        if (end) {
            emit_2arg(compiler, SIE_OP_CMP, var, end);
            emit_branch(compiler, SIE_OP_BLE, loop_end);
        }
        compile_children(compiler, node);
        if (!zero_increment_p)
            emit_3arg(compiler, SIE_OP_ADD, var, var, increment);
        emit_branch(compiler, SIE_OP_B, loop_down_start);
    }

    if (end)
        emit_label(compiler, loop_end);
}

static void compile_read(sie_Compiler *compiler, sie_XML *node)
{
    const char *var      = get_optional_arg(node, "var", SIE_NAME);
    const char *bits_s   = get_optional_arg(node, "bits", SIE_NOT_NAME);
    const char *octets_s = get_optional_arg(node, "octets", SIE_NOT_NAME);
    const char *type_s   = get_optional_arg(node, "type", SIE_NAME);
    const char *endian_s = get_optional_arg(node, "endian", SIE_NAME);
    const char *value_s  = get_optional_arg(node, "value", SIE_NOT_NAME);

    int saved;
    int op = 1;
    int bits = 0;
    
    if (bits_s && octets_s)
        sie_errorf((compiler, "both bits and octets present"));
    if (!bits_s && !octets_s)
        sie_errorf((compiler, "full payload reads not supported yet"));
    if (!type_s)
        type_s = "raw";
    
    if (!strcmp(type_s, "raw")) {
        if (bits_s) sie_errorf((compiler, "raw needs octects for now"));
        emit_2arg(compiler, SIE_OP_READ_RAW, var, octets_s);
        return;
    }

    saved = save_temp_registers(compiler);
    if (!var)
        var = next_temp_register(compiler);

    /* KLUDGE - constants only! */
    if (bits_s)   bits = sie_strtoint(compiler, bits_s);
    if (octets_s) bits = sie_strtoint(compiler, octets_s) * 8;

    switch (bits) {
    case 8: break;
    case 16: op += 1; break;
    case 32: op += 2; break;
    case 64: op += 3; break;
    default: sie_errorf((compiler, "unsupported bit size '%d'", bits));
    }
                   
    if (!endian_s) {
        if (bits == 8)
            endian_s = "big";
        else
            sie_errorf((compiler, "endian required for number types"));
    }
    if (!strcmp(endian_s, "little")) op += 0;
    else if (!strcmp(endian_s, "big")) op += 10;
    else sie_errorf((compiler, "unknown endian '%s'", endian_s));

    if (!strcmp(type_s, "uint")) op += 0;
    else if (!strcmp(type_s, "int")) op += 4;
    else if (!strcmp(type_s, "float")) {
        op += 6;
        if (!(bits == 32 || bits == 64))
            sie_errorf((compiler, "unsupported size for floats"));
    } else {
        sie_errorf((compiler, "unsupported read type '%s'", type_s));
    }
    
    emit_1arg(compiler, op, var);
    if (value_s)
        emit_2arg(compiler, SIE_OP_ASSERT, var, value_s);
    restore_temp_registers(compiler, saved);
}

static void compile_seek(sie_Compiler *compiler, sie_XML *node)
{
    const char *offset = get_required_arg(node, "offset", SIE_NOT_NAME);
    const char *from = sie_xml_get_attribute(node, "from");
    char *whence = "";

    if (!strcmp(from, "start"))
        whence = "0";
    else if (!strcmp(from, "current"))
        whence = "1";
    else if (!strcmp(from, "end"))
        whence = "2";
    else
        sie_errorf((compiler, "from not one of start, current, end"));

    emit_2arg(compiler, SIE_OP_SEEK, offset, whence);
}

static void compile_sample(sie_Compiler *compiler, sie_XML *node)
{
    emit_0arg(compiler, SIE_OP_SAMPLE);
}

static void compile_if(sie_Compiler *compiler, sie_XML *node)
{
    const char *cond = get_optional_arg(node, "condition", SIE_NOT_NAME);

    int skip = next_label(compiler);

    emit_2arg(compiler, SIE_OP_CMP, cond, "0");
    emit_branch(compiler, SIE_OP_BEQ, skip);
    compile_children(compiler, node);
    emit_label(compiler, skip);
}

static const struct {
    const char *name;
    void (*fn)(sie_Compiler *, sie_XML *);
} compile_fns[] = {
    { "decoder", compile_decoder },
    { "set", compile_set },
    { "loop", compile_loop },
    { "read", compile_read },
    { "seek", compile_seek },
    { "sample", compile_sample },
    { "if", compile_if },
};

static const int num_compile_fns =
    sizeof(compile_fns) / sizeof(*compile_fns);

static void compile_node(sie_Compiler *compiler, sie_XML *node)
{
    const char *name = sie_string_value(node->value.element.name);
    int i;
    if (node->type != SIE_XML_ELEMENT)
        return;
    sie_error_context_push(compiler, "Compiling node '%s'", name);
    for (i = 0; i < num_compile_fns; i++) {
        if (!strcmp(name, compile_fns[i].name)) {
            compile_fns[i].fn(compiler, node);
            sie_error_context_pop(compiler);
            return;
        }
    }
    sie_errorf((compiler, "no compiler for node '%s'", name));
}

static const char *compile_expr_value(sie_Compiler *compiler,
                                      const char *return_reg,
                                      sie_XML *node)
{
    const char *value = sie_xml_get_attribute(node, "n");
    sie_assert(value, compiler);
    return value;
}

#define MATH_OP_COMPILE_BINARY(name, op)                                \
    static const char *compile_expr_##name (sie_Compiler *compiler,     \
                                            const char *return_reg,     \
                                            sie_XML *node)              \
    {                                                                   \
        int saved = save_temp_registers(compiler);                      \
        const char *a1 =                                                \
            compile_expr_node(compiler, next_temp_register(compiler),   \
                              node->child);                             \
        const char *a2 =                                                \
            compile_expr_node(compiler, next_temp_register(compiler),   \
                              node->child->next);                       \
        emit_3arg(compiler, op, return_reg, a1, a2);                    \
        restore_temp_registers(compiler, saved);                        \
        return return_reg;                                              \
    }

#define MATH_OP_COMPILE_UNARY(name, op)                                 \
    static const char *compile_expr_##name (sie_Compiler *compiler,     \
                                            const char *return_reg,     \
                                            sie_XML *node)              \
    {                                                                   \
        const char *a =                                                 \
            compile_expr_node(compiler, NULL, node->child);             \
        emit_2arg(compiler, op, return_reg, a);                         \
        return return_reg;                                              \
    }

MATH_OP_COMPILE_BINARY(add, SIE_OP_ADD);
MATH_OP_COMPILE_BINARY(sub, SIE_OP_SUB);
MATH_OP_COMPILE_BINARY(mul, SIE_OP_MUL);
MATH_OP_COMPILE_BINARY(div, SIE_OP_DIV);
MATH_OP_COMPILE_BINARY(and, SIE_OP_AND);
MATH_OP_COMPILE_BINARY(or, SIE_OP_OR);
MATH_OP_COMPILE_BINARY(lsl, SIE_OP_LSL);
MATH_OP_COMPILE_BINARY(lsr, SIE_OP_LSR);

MATH_OP_COMPILE_UNARY(lnot, SIE_OP_LNOT);

MATH_OP_COMPILE_BINARY(lt, SIE_OP_LT);
MATH_OP_COMPILE_BINARY(le, SIE_OP_LE);
MATH_OP_COMPILE_BINARY(gt, SIE_OP_GT);
MATH_OP_COMPILE_BINARY(ge, SIE_OP_GE);
MATH_OP_COMPILE_BINARY(eq, SIE_OP_EQ);
MATH_OP_COMPILE_BINARY(ne, SIE_OP_NE);
MATH_OP_COMPILE_BINARY(land, SIE_OP_LAND);
MATH_OP_COMPILE_BINARY(lor, SIE_OP_LOR);

static const struct {
    const char *name;
    const char *(*fn)(sie_Compiler *, const char *, sie_XML *);
} compile_expr_fns[] = {
    { "v", compile_expr_value },
    { "+", compile_expr_add },
    { "-", compile_expr_sub },
    { "*", compile_expr_mul },
    { "/", compile_expr_div },
    { "&", compile_expr_and },
    { "|", compile_expr_or },
    { "<<", compile_expr_lsl },
    { ">>", compile_expr_lsr },
    { "!", compile_expr_lnot },
    { "<", compile_expr_lt },
    { "<=", compile_expr_le },
    { ">", compile_expr_gt },
    { ">=", compile_expr_ge },
    { "==", compile_expr_eq },
    { "!=", compile_expr_ne },
    { "&&", compile_expr_land },
    { "||", compile_expr_lor },
};

static const int num_compile_expr_fns =
    sizeof(compile_expr_fns) / sizeof(*compile_expr_fns);

static const char *compile_expr_node(sie_Compiler *compiler,
                                     const char *return_reg,
                                     sie_XML *node)
{
    const char *name = sie_string_value(node->value.element.name);
    const char *retval = "";
    int i;
    int saved = save_temp_registers(compiler);
    sie_assert(node->type == SIE_XML_ELEMENT, compiler);
    sie_error_context_push(compiler, "Compiling expr node '%s'", name);
    if (return_reg == NULL)
        return_reg = next_temp_register(compiler);
    for (i = 0; i < num_compile_expr_fns; i++) {
        if (!strcmp(name, compile_expr_fns[i].name)) {
            retval = compile_expr_fns[i].fn(compiler, return_reg, node);
            sie_error_context_pop(compiler);
            goto done;
        }
    }
    sie_errorf((compiler, "no compiler for expr node '%s'", name));
done:
    restore_temp_registers(compiler, saved);
    return retval;
}

void sie_compiler_compile_into_decoder(sie_Decoder *decoder, sie_XML *source)
{
    size_t mark = sie_cleanup_mark(decoder);
    sie_Compiler *compiler = sie_autorelease(sie_compiler_new(decoder));
    int num_bytecodes;
    int bytecode_bytes;
    int num_registers;
    int register_bytes;
    sie_vec(vs, int, 0);
    char v_name[32];
    int v = 0;

    sie_vec_autofree(decoder, &vs);

    compile_node(compiler, source);
    fixup_labels(compiler);

    num_bytecodes = (int)sie_vec_size(compiler->bytecode);
    bytecode_bytes = num_bytecodes * sie_vec_el_size(compiler->bytecode);
    num_registers = (int)sie_vec_size(compiler->initial_registers);
    register_bytes =
        num_registers * sie_vec_el_size(compiler->initial_registers);

    decoder->num_bytecodes = num_bytecodes;
    decoder->bytecode = sie_malloc(decoder, bytecode_bytes);
    memcpy(decoder->bytecode, compiler->bytecode, bytecode_bytes);
    
    decoder->num_registers = num_registers;
    decoder->initial_registers = sie_malloc(decoder, register_bytes);
    memcpy(decoder->initial_registers, compiler->initial_registers,
           register_bytes);

    decoder->register_names = sie_rel_clone(compiler->register_names);

    for (;;) {
        int v_reg;
        sprintf(v_name, "v%d", v);
        if (!sie_rel_int_value(compiler->register_names, v_name, &v_reg))
            break;
        sie_vec_push_back(vs, v_reg);
        v++;
    }
    
    if ( (decoder->num_vs = sie_vec_size(vs)) ) {
        decoder->vs =
            sie_malloc(decoder, sie_vec_size(vs) * sie_vec_el_size(vs));
        memcpy(decoder->vs, vs, sie_vec_size(vs) * sie_vec_el_size(vs));
    }

    sie_cleanup_pop_mark(decoder, mark);
}
