/**
 Implementation notes

 This is currently not error-tolerant, at all. Post-jam, making the parser fully
tolerant is extremely important, and a high-priority TODO.
 */

#include "ast.h"
#include "util/string.h"
#include "stb_ds.h"

#include <inttypes.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef noreturn
#define coy_noreturn noreturn
#elif __STDC_VERSION__ >= 201112L
#define coy_noreturn _Noreturn
#elif __GNUC__ 
#define coy_noreturn __attribute__ ((noreturn))
#else
#define coy_noreturn 
#endif

coy_noreturn void errorf(coyc_pctx_t *ctx, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(NULL, 0, fmt, args);
    char *buf = malloc(len + 1);
    vsprintf(buf, fmt, args);
    va_end(args);
    ctx->err_msg = buf;
    fprintf(stderr, "Parser: %s\n", buf);
    longjmp(ctx->err_env, 255);
}

coy_noreturn void error(coyc_pctx_t *ctx, const char *msg) {
    errorf(ctx, "%s", msg);
}

coy_noreturn void error_token(coyc_pctx_t *ctx, coyc_token_t token, const char *fmt) {
    char buf[128];
    size_t len = token.len < 128 ? token.len : 127;
    strncpy(buf, token.ptr, len);
    buf[len] = 0;
    if (token.len != len) {
        fprintf(stderr, "Warning: token len exceeded 128, truncated.\n");
    }
    errorf(ctx, fmt, buf);
}

type_t coyc_type(coyc_token_t token) {
    type_t type;
    type.primitive = invalid;
    if (strncmp(token.ptr, "int", token.len) == 0) {
        type.primitive = _int;
    }
    return type;
}

static void parse_module(coyc_pctx_t *ctx)
{
    if (ctx->tokens[0].kind != COYC_TK_MODULE) {
        error(ctx, "Missing a module statement!");
    }
    
    if (ctx->root->module_name) {
        error(ctx, "Invalid `module` statement found!");
    }

    if (ctx->tokens[1].kind != COYC_TK_IDENT) {
        error(ctx, "Expected identifier for module name!");
    }

    ctx->root->module_name = coyc_token_read(ctx->tokens[1]);

    if (ctx->tokens[2].kind != COYC_TK_SCOLON) {
        error(ctx, "Expected semicolon after module statement!");
    }
    ctx->token_index = 3;
}

static expression_value_t compute_atom(coyc_pctx_t *ctx, unsigned int minimum_prec) {
    if (ctx->token_index > arrlenu(ctx->tokens)) {
        error(ctx, "Unexpected EOF!");
    }
    coyc_token_t token = ctx->tokens[ctx->token_index];
    if (token.kind == COYC_TK_LPAREN) {
        error(ctx, "TODO parens");
    }
    if (token.kind == COYC_TK_INTEGER) {
        expression_value_t val;
        val.literal.type = literal;
        val.literal.value.integer.type.primitive = uint;
        // TODO: error checking
        sscanf(token.ptr, "%" SCNu64, &val.literal.value.integer.value);
        ctx->token_index += 1;
        return val;
    }
    error(ctx, "TODO more atoms");
    // Not yet used.
    (void)minimum_prec;
}

typedef struct {
    coyc_token_kind_t token;
    unsigned prec;
    enum {left,right} assoc;
} op_t;

op_t ops[1] = {
   { .token = COYC_TK_OPADD, .prec = 1, .assoc = left } 
};

static op_t *find_op(coyc_token_t token) {
    for (size_t i = 0; i < sizeof(ops) / sizeof(op_t); i += 1) {
        if (ops[i].token == token.kind) {
            return ops + i;
        }
    }
    return NULL;
}

static void expression_free(expression_t *expression) {
    free(expression);
}

static expression_t *parse_expression(coyc_pctx_t *ctx, unsigned int minimum_prec) {
    (void)minimum_prec;
    expression_t *expr = malloc(sizeof(expression_t));
    expr->lhs = compute_atom(ctx, minimum_prec);
    expr->rhs.type = none;
    // TODO
    expr->type = expr->lhs.literal.value.integer.type;
    while (true) {
        coyc_token_t token = ctx->tokens[ctx->token_index];
        expr->op = token;
        if (token.kind == COYC_TK_SCOLON) {
            expr->op = token;
            return expr;
        }
        const op_t *op = find_op(token);
        if (op == NULL || op->prec < minimum_prec) {
            // Done
            return expr;
        }
        const int next_prec = op->assoc == left ? op->prec + 1 : op->prec;
        ctx->token_index += 1;
        expression_t *rhs = parse_expression(ctx, next_prec);
        if (expr->rhs.type == none && rhs->rhs.type == none && rhs->op.kind == COYC_TK_SCOLON && expr->op.kind == COYC_TK_OPADD && expr->lhs.type == literal && rhs->lhs.type == literal) {
            // TODO types
            expr->lhs.literal.value.integer.value += rhs->lhs.literal.value.integer.value;
            expr->op = rhs->op;
            expression_free(rhs);
        }
        else {
            *((volatile int*)NULL) = 0;
        }
    }
}

// Returns true if there's no more instructions
static bool parse_statement(coyc_pctx_t *ctx, function_t *function)
{
    coyc_token_t token = ctx->tokens[ctx->token_index];
    if (token.kind == COYC_TK_EOF) {
        error(ctx, "Error: expected '}', found EOF!");
    }
    if (token.kind == COYC_TK_ERROR) {
        error(ctx, "Error in lexer while parsing function body!");
    }
    if (token.kind == COYC_TK_RBRACE) {
        ctx->token_index += 1;
        return true;
    }
    if (token.kind == COYC_TK_RETURN) {
        ctx->token_index += 1;
        token = ctx->tokens[ctx->token_index];
        statement_t return_stmt;
        return_stmt.return_.type = return_;
        return_stmt.return_.value = parse_expression(ctx, 1);
        // Skip the semicolon
        ctx->token_index += 1;
        arrput(function->statements, return_stmt);
    }
    else {
        errorf(ctx, "TODO parse statement %s", coyc_token_kind_tostr_DBG(token.kind));
    }
    return false;
}

/// token_index must mark the first token after the LPAREN
static void parse_function(coyc_pctx_t *ctx, type_t type, char *ident)
{
    decl_t decl;
    decl.function.base.type = function;
    decl.function.base.name = ident;
    decl.function.statements = NULL;
    decl.function.return_type = type;
    coyc_token_t token = ctx->tokens[ctx->token_index];
    if (token.kind != COYC_TK_RPAREN) {
        error(ctx, "TODO support parsing parameter list");
    }
    ctx->token_index += 1;
    token = ctx->tokens[ctx->token_index];
    if (token.kind != COYC_TK_LBRACE) {
        errorf(ctx, "Error: expected function body, found %s", coyc_token_kind_tostr_DBG(token.kind));
    }
    ctx->token_index += 1;

    while (!parse_statement(ctx, &decl.function));
    
    arrput(ctx->root->decls, decl);
}

static void parse_decl(coyc_pctx_t *ctx)
{
    coyc_token_t token = ctx->tokens[ctx->token_index];
    if (token.kind == COYC_TK_TYPE) {
        const type_t type = coyc_type(token);
        if (type.primitive == invalid) {
            error_token(ctx, token, "TODO parse type %s");
        }
        ctx->token_index += 1;
        token = ctx->tokens[ctx->token_index];
        if (token.kind == COYC_TK_IDENT) {
            char *ident = coyc_token_read(token);
            ctx->token_index += 1;
            token = ctx->tokens[ctx->token_index];
            if (token.kind == COYC_TK_LPAREN) {
                ctx->token_index += 1;
                parse_function(ctx, type, ident);
            }
            else {
                errorf(ctx, "TODO parser TYPE IDENT %s", coyc_token_kind_tostr_DBG(token.kind));
            }
        }
        else {
            errorf(ctx, "TODO parser TYPE %s", coyc_token_kind_tostr_DBG(token.kind));
        }
    }
    else {
        errorf(ctx, "TODO parse %s", coyc_token_kind_tostr_DBG(token.kind));    
    }
}

void coyc_parse(coyc_pctx_t *ctx)
{
    if (!ctx) return;
    coyc_lexer_t *lexer = ctx->lexer;
    if (!lexer) return;
    ast_root_t *root = ctx->root;
    if (!root) return;

    root->decls = NULL;
    root->module_name = NULL;

    ctx->err_msg = NULL;
    ctx->root = root;
    ctx->tokens = NULL;
    ctx->token_index = 0;

    for(;;){
        coyc_token_t token = coyc_lexer_next(lexer, COYC_LEXER_CATEGORY_PARSER);
        if(token.kind == COYC_TK_EOF)
            break;
        arrput(ctx->tokens, token);
    }

    if (arrlen(ctx->tokens) == 0) {
        coyc_tree_free(ctx);
        return;
    }

    if (setjmp(ctx->err_env) == 255)
    {
        // Error
        return;
    }

    parse_module(ctx);
    while (ctx->token_index < arrlenu(ctx->tokens)) {
        parse_decl(ctx);
    }

}

void coyc_tree_free(coyc_pctx_t *ctx)
{
    arrfree(ctx->tokens);
    if (ctx->err_msg) {
        free(ctx->err_msg);
    }
    if (ctx->root)
    {
        if (ctx->root->module_name) {
            free(ctx->root->module_name);
            ctx->root->module_name = NULL;
        }
        if (ctx->root->decls) {
            for (int i = 0; i < arrlen(ctx->root->decls); i++) {
                decl_t *decl = &ctx->root->decls[i];
                free(decl->base.name);
                decl->base.name = NULL;
                if (decl->base.type == function) {
                    if (decl->function.statements) {
                        arrfree(decl->function.statements);
                    }
                }
            }
            arrfree(ctx->root->decls);
        }
    }
}
