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

#include "sie_xml.h"

#include "sie_utils.h"
#include "sie_parser.h"
#include "sie_vec.h"

typedef enum sie_token_type sie_token_type;
enum sie_token_type {
    SIE_TOKEN_ANY = 0,
    SIE_TOKEN_PLUS,
    SIE_TOKEN_MINUS,
    SIE_TOKEN_STAR,
    SIE_TOKEN_SLASH,
    SIE_TOKEN_PERCENT,
    SIE_TOKEN_LPAREN,
    SIE_TOKEN_RPAREN,
    SIE_TOKEN_DOLLAR,
    SIE_TOKEN_AMP,
    SIE_TOKEN_AMPAMP,
    SIE_TOKEN_BAR,
    SIE_TOKEN_BARBAR,
    SIE_TOKEN_LESSLESS,
    SIE_TOKEN_GREATERGREATER,
    SIE_TOKEN_LCURLY,
    SIE_TOKEN_RCURLY,
    SIE_TOKEN_BANG,
    SIE_TOKEN_LESS,
    SIE_TOKEN_GREATER,
    SIE_TOKEN_LESSEQUAL,
    SIE_TOKEN_GREATEREQUAL,
    SIE_TOKEN_EQUALEQUAL,
    SIE_TOKEN_BANGEQUAL,

    SIE_TOKEN_NUMBER,
    SIE_TOKEN_NAME,
};

static const struct {
    const char *string;
    sie_token_type type;
} ops[] = {
    /* Longer operators must come first. */
    { "<=", SIE_TOKEN_LESSEQUAL },
    { ">=", SIE_TOKEN_GREATEREQUAL },
    { "==", SIE_TOKEN_EQUALEQUAL },
    { "!=", SIE_TOKEN_BANGEQUAL },
    { "&&", SIE_TOKEN_AMPAMP },
    { "||", SIE_TOKEN_BARBAR },
    { "<<", SIE_TOKEN_LESSLESS },
    { ">>", SIE_TOKEN_GREATERGREATER },
    { "+",  SIE_TOKEN_PLUS },
    { "-",  SIE_TOKEN_MINUS },
    { "*",  SIE_TOKEN_STAR },
    { "/",  SIE_TOKEN_SLASH },
    { "%",  SIE_TOKEN_PERCENT },
    { "&",  SIE_TOKEN_AMP },
    { "|",  SIE_TOKEN_BAR },
    { "(",  SIE_TOKEN_LPAREN },
    { ")",  SIE_TOKEN_RPAREN },
    { "{",  SIE_TOKEN_LCURLY },
    { "}",  SIE_TOKEN_RCURLY },
    { "$",  SIE_TOKEN_DOLLAR },
    { "!",  SIE_TOKEN_BANG },
    { "<",  SIE_TOKEN_LESS },
    { ">",  SIE_TOKEN_GREATER },
};

typedef struct _sie_token sie_token;
struct _sie_token {
    sie_token_type type;
    char string[2048]; /* MEGA KLUDGE!!!!!!! */
    char *op_name;
    int position;
};

typedef struct _sie_lexer sie_lexer;
struct _sie_lexer {
    void *ctx_obj;
    const char *string;
    int length;
    int position;
    sie_token lookahead;
};

void token(sie_lexer *lx, sie_token_type type,
           const char *string, int position)
{
    lx->lookahead.type = type;
    strcpy(lx->lookahead.string, string);
    lx->lookahead.position = position;
}

int lexer(sie_lexer *lx)
{
    int pos = lx->position;
    char s[1024];               /* KLUDGE unsafe! */
    const char *str;
    size_t i;
    double d;
    int n;

    lx->lookahead.type = 0;
    
    if (pos >= lx->length) return 0;

    pos += sie_whitespace(&lx->string[pos]);
    str = &lx->string[pos];

    for (i = 0; i < sizeof(ops) / sizeof(*ops); i++) {
        if (!strncmp(ops[i].string, str, strlen(ops[i].string))) {
            token(lx, ops[i].type, ops[i].string, pos);
            lx->position = pos + (int)strlen(ops[i].string);
            return 1;
        }
    }
    if (sie_scan_number(str, &d, &n)) {
        strncpy(s, str, n);
        s[n] = 0;
        token(lx, SIE_TOKEN_NUMBER, s, pos);
        lx->position = pos + n;
        return 1;
    }
    if (sscanf(str, "%[A-Za-z0-9_]%n", s, &n) == 1) {
        /* This works because anything starting with 0-9 would have
         * already been consumed by the previous scan. */
        token(lx, SIE_TOKEN_NAME, s, pos);
        lx->position = pos + n;
        return 1;
    }

    lx->position = pos;

    if (pos < lx->length)
        sie_errorf((lx->ctx_obj,
                    "Lexer failure:\n"
                    "  : \"%s\"\n"
                    "at:  %*s^\n",
                    lx->string,
                    pos, ""));

    return 0;
}

typedef struct _sie_Parser sie_Parser;
struct _sie_Parser {
    sie_Context_Object parent;
    sie_lexer *lexer;
    sie_XML *cleanup;
};
SIE_CLASS_DECL(sie_Parser);
#define SIE_PARSER(p) SIE_SAFE_CAST(p, sie_Parser);

static void sie_parser_init(sie_Parser *self, void *ctx_obj, sie_lexer *lexer)
{
    sie_context_object_init(SIE_CONTEXT_OBJECT(self), ctx_obj);
    self->lexer = lexer;
    self->cleanup = sie_xml_new_element(self, "cleanup");
}

static void sie_parser_destroy(sie_Parser *self)
{
    sie_release(self->cleanup);
    sie_context_object_destroy(SIE_CONTEXT_OBJECT(self));
}

SIE_CONTEXT_OBJECT_NEW_FN(sie_parser_new, sie_Parser, self, ctx_obj,
                          (void *ctx_obj, sie_lexer *lexer),
                          sie_parser_init(self, ctx_obj, lexer));

SIE_CLASS(sie_Parser, sie_Context_Object,
          SIE_MDEF(sie_destroy, sie_parser_destroy));


static void parser_lose(sie_Parser *p)
{
    sie_errorf((p,
                "Parse failure:\n"
                "  : \"%s\"\n"
                "at:  %*s^\n",
                p->lexer->string,
                p->lexer->lookahead.position, ""));
}

static sie_XML *p_xml_new_element(sie_Parser *p, const char *name)
{
    sie_XML *node = sie_xml_new_element(p, name);
    sie_xml_append(node, p->cleanup);
    return node;
}

static sie_XML *p_xml_append(sie_XML *node, sie_XML *parent)
{
    return sie_xml_append(sie_xml_extract(node), parent);
}

static int check(sie_Parser *p, sie_token_type type)
{
    return (p->lexer->lookahead.type == type ||
            type == SIE_TOKEN_ANY);
}

static void match(sie_Parser *p, sie_token_type type)
{
    if (check(p, type)) {
        lexer(p->lexer);
    } else {
        parser_lose(p);
    }
}

static sie_XML *parse_expression(sie_Parser *p);

static sie_XML *parse_factor(sie_Parser *p)
{
    sie_XML *tree;

    switch (p->lexer->lookahead.type) {
    case SIE_TOKEN_LPAREN:
        match(p, SIE_TOKEN_LPAREN);
        tree = parse_expression(p);
        match(p, SIE_TOKEN_RPAREN);
        break;
    case SIE_TOKEN_NUMBER:
        tree = p_xml_new_element(p, "v");
        sie_xml_set_attribute(tree, "n", p->lexer->lookahead.string);
        match(p, SIE_TOKEN_NUMBER);
        break;
    case SIE_TOKEN_DOLLAR:
        match(p, SIE_TOKEN_DOLLAR);
        tree = p_xml_new_element(p, "v");
        sie_xml_set_attribute(tree, "n", p->lexer->lookahead.string);
        match(p, SIE_TOKEN_NAME);
        break;
    case SIE_TOKEN_NAME:
        tree = p_xml_new_element(p, "apply");
        sie_xml_set_attribute(tree, "f", p->lexer->lookahead.string);
        match(p, SIE_TOKEN_NAME);
        match(p, SIE_TOKEN_LPAREN);
        p_xml_append(parse_expression(p), tree);
        match(p, SIE_TOKEN_RPAREN);
        break;
    case SIE_TOKEN_PLUS:
        match(p, SIE_TOKEN_PLUS);
        tree = parse_factor(p);
        break;
    case SIE_TOKEN_MINUS:
        match(p, SIE_TOKEN_MINUS);
        {
            sie_XML *zero = p_xml_new_element(p, "v");
            sie_xml_set_attribute(zero, "n", "0");
            tree = p_xml_new_element(p, "-");
            p_xml_append(zero, tree);
            p_xml_append(parse_factor(p), tree);
        }
        break;
    case SIE_TOKEN_BANG:
        match(p, SIE_TOKEN_BANG);
        tree = p_xml_new_element(p, "!");
        p_xml_append(parse_factor(p), tree);
        break;
    default:
        parser_lose(p);
        return NULL;
    }

    return tree;
}

static sie_XML *parse_binary_4types(sie_Parser *p,
                                    sie_XML *(next)(sie_Parser *),
                                    sie_token_type type1,
                                    sie_token_type type2,
                                    sie_token_type type3,
                                    sie_token_type type4)
{
    sie_XML *tree = next(p);
    
    while (check(p, type1) || check(p, type2) ||
           check(p, type3) || check(p, type4)) {
        sie_XML *new_tree = p_xml_new_element(p, p->lexer->lookahead.string);
        p_xml_append(tree, new_tree);
        match(p, SIE_TOKEN_ANY);
        p_xml_append(next(p), new_tree);
        tree = new_tree;
    }
    
    return tree;
}

static sie_XML *parse_binary_3types(sie_Parser *p,
                                    sie_XML *(next)(sie_Parser *),
                                    sie_token_type type1,
                                    sie_token_type type2,
                                    sie_token_type type3)
{
    return parse_binary_4types(p, next, type1, type2, type3, type3);
}

static sie_XML *parse_binary_2types(sie_Parser *p,
                                    sie_XML *(next)(sie_Parser *),
                                    sie_token_type type1,
                                    sie_token_type type2)
{
    return parse_binary_4types(p, next, type1, type2, type2, type2);
}

static sie_XML *parse_binary_1type(sie_Parser *p,
                                   sie_XML *(next)(sie_Parser *),
                                   sie_token_type type)
{
    return parse_binary_4types(p, next, type, type, type, type);
}

static sie_XML *parse_multiply(sie_Parser *p)
{
    return parse_binary_3types(p, parse_factor,
                               SIE_TOKEN_STAR,
                               SIE_TOKEN_SLASH,
                               SIE_TOKEN_PERCENT);
}

static sie_XML *parse_add(sie_Parser *p)
{
    return parse_binary_2types(p, parse_multiply,
                               SIE_TOKEN_PLUS, SIE_TOKEN_MINUS);
}

static sie_XML *parse_shift(sie_Parser *p)
{
    return parse_binary_2types(p, parse_add,
                               SIE_TOKEN_LESSLESS,
                               SIE_TOKEN_GREATERGREATER);
}

static sie_XML *parse_compare_range(sie_Parser *p)
{
    return parse_binary_4types(p, parse_shift,
                               SIE_TOKEN_LESS,
                               SIE_TOKEN_GREATER,
                               SIE_TOKEN_LESSEQUAL,
                               SIE_TOKEN_GREATEREQUAL);
}

static sie_XML *parse_compare_single(sie_Parser *p)
{
    return parse_binary_2types(p, parse_compare_range,
                               SIE_TOKEN_EQUALEQUAL,
                               SIE_TOKEN_BANGEQUAL);
}

static sie_XML *parse_bit_and(sie_Parser *p)
    { return parse_binary_1type(p, parse_compare_single, SIE_TOKEN_AMP); }

static sie_XML *parse_bit_or(sie_Parser *p)
    { return parse_binary_1type(p, parse_bit_and, SIE_TOKEN_BAR); }

static sie_XML *parse_and(sie_Parser *p)
    { return parse_binary_1type(p, parse_bit_or, SIE_TOKEN_AMPAMP); }

static sie_XML *parse_or(sie_Parser *p)
    { return parse_binary_1type(p, parse_and, SIE_TOKEN_BARBAR); }

static sie_XML *parse_expression(sie_Parser *p)
    { return parse_or(p); }

static sie_XML *parse_whole_expression(sie_Parser *p)
{ 
    sie_XML *tree;
    match(p, SIE_TOKEN_LCURLY);
    tree = parse_expression(p);
    match(p, SIE_TOKEN_RCURLY);
    return tree;
}

sie_XML *parse_text_expression(void *ctx_obj, const char *text)
{
    /* KLUDGEY */
    size_t mark = sie_cleanup_mark(ctx_obj);
    sie_lexer lx = { ctx_obj, NULL, 0, 0, { 0, { 0 }, NULL, 0 } };
    sie_Parser *p = sie_autorelease(sie_parser_new(ctx_obj, &lx));
    sie_XML *retval;

    sie_error_context_auto(ctx_obj, "Parsing expression '%s'", text);
    lx.string = text;
    lx.length = (int)strlen(text);
    lx.position = 0;
    lexer(p->lexer);
    
    retval = sie_xml_extract(parse_whole_expression(p));

    sie_cleanup_pop_mark(ctx_obj, mark);

    return retval;
}
