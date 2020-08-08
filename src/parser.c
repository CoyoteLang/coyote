/**
 Implementation notes

 This is currently not error-tolerant, at all. Post-jam, making the parser fully
tolerant is extremely important, and a high-priority TODO.
 */

#include "ast.h"
#include "stb_ds.h"

#include <inttypes.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void error(coyc_pctx_t *ctx, const char *msg) {
    fprintf(stderr, "Parser: %s\n", msg);
    ctx->err_msg = strdup(msg);
    longjmp(ctx->err_env, 255);
}

void errorf(coyc_pctx_t *ctx, const char *fmt, ...) {
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

void error_token(coyc_pctx_t *ctx, coyc_token_t token, const char *fmt) {
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

// Returns true if there's no more instructions
static bool parse_instruction(coyc_pctx_t *ctx, function_t *function)
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
        if (token.kind == COYC_TK_INTEGER) {
            instruction_t instruction;
            sscanf(token.ptr, "%" SCNu64, &instruction.return_value.constant);
            arrput(function->instructions, instruction);
            ctx->token_index += 1;
            token = ctx->tokens[ctx->token_index];
            if (token.kind != COYC_TK_SCOLON) {
                error(ctx, "TODO return INT [^;]");
            }
            ctx->token_index += 1;
            return false;
        }
        else {
            errorf(ctx, "TODO parser.c:%d", __LINE__);
        }
    }
    else {
        errorf(ctx, "TODO parse function instruction %s", coyc_token_kind_tostr_DBG(token.kind));
    }
    return false;
}

/// token_index must mark the first token after the LPAREN
static void parse_function(coyc_pctx_t *ctx, type_t type, char *ident)
{
    function_t function;
    function.instructions = NULL;
    function.name = ident;
    function.return_type = type;
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

    while (!parse_instruction(ctx, &function));

    arrput(ctx->root->functions, function);
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
    root_node_t *root = ctx->root;
    if (!root) return;

    root->imports = NULL;
    root->module_name = NULL;
    root->functions = NULL;

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
        if (ctx->root->functions) {
            for (int i = 0; i < arrlen(ctx->root->functions); i++) {
                function_t *func = &ctx->root->functions[i];
                if (func->name) {
                    free(func->name);
                    func->name = NULL;
                }
                if (func->instructions) {
                    arrfree(func->instructions);
                }
            }
            arrfree(ctx->root->functions);
        }
    }
}
