/**
 Implementation notes

 This is currently not error-tolerant, at all. Post-jam, making the parser fully
tolerant is extremely important, and a high-priority TODO.
 */

#include "ast.h"
#include "util/string.h"
#include "util/hints.h"
#include "stb_ds.h"

#include <inttypes.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static COY_HINT_NORETURN COY_HINT_PRINTF(2, 3) void errorf(coyc_pctx_t *ctx, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char *buf = coy_vaprintf_(fmt, args);
    va_end(args);
    ctx->err_msg = buf;
    fprintf(stderr, "Parser: %s\n", buf);
    longjmp(ctx->err_env, 255);
}

#define ERROR(msg) do { errorf(ctx, "%s at %s:%d", msg, __FILE__, __LINE__); } while (0);

static COY_HINT_NORETURN void error_token(coyc_pctx_t *ctx, coyc_token_t token, const char *fmt) {
    char buf[128];
    size_t len = token.len < 128 ? token.len : 127;
    strncpy(buf, token.ptr, len);
    buf[len] = 0;
    if (token.len != len) {
        fprintf(stderr, "Warning: token len exceeded 128, truncated.\n");
    }
    errorf(ctx, fmt, buf);
}

static type_t coyc_type(coyc_token_t token) {
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
        ERROR("Missing a module statement!");
    }
    
    if (arrlenu(ctx->tokens) == 1) {
        ERROR("No token after `module` statement!");
    }

    if (ctx->tokens[1].kind != COYC_TK_IDENT) {
        ERROR("Expected identifier for module name!");
    }

    ctx->root->module_name = coyc_token_read(ctx->tokens[1]);

    if (ctx->tokens[2].kind != COYC_TK_SCOLON) {
        ERROR("Expected semicolon after module statement!");
    }
    ctx->token_index = 3;
}

static expression_t *parse_expression(coyc_pctx_t *ctx, unsigned int minimum_prec);

static expression_value_t compute_atom(coyc_pctx_t *ctx, unsigned int minimum_prec) {
    if (ctx->token_index > arrlenu(ctx->tokens)) {
        ERROR("Unexpected EOF!");
    }
    coyc_token_t token = ctx->tokens[ctx->token_index];
    if (token.kind == COYC_TK_LPAREN) {
        ctx->token_index += 1;
        expression_value_t val;
        val.expression.type = expression;
        val.expression.expression = parse_expression(ctx, 1);
        if (ctx->tokens[ctx->token_index].kind != COYC_TK_RPAREN) {
            ERROR("Missing right parentheses");            
        }
        else {
            ctx->token_index += 1;
        }
        return val;
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
    if (token.kind == COYC_TK_IDENT) {
        expression_value_t val;
        val.identifier.type = identifier;
        val.identifier.name = coyc_token_read(token);
        ctx->token_index += 1;
        return val;
    }
    ERROR("TODO more atoms");
    // Not yet used.
}

typedef struct {
    coyc_token_kind_t token;
    unsigned prec;
    enum {left,right} assoc;
} op_t;

op_t ops[4] = {
   { .token = COYC_TK_OPADD, .prec = 1, .assoc = left },
   { .token = COYC_TK_OPSUB, .prec = 1, .assoc = left },
   { .token = COYC_TK_OPDIV, .prec = 2, .assoc = left },
   { .token = COYC_TK_OPMUL, .prec = 2, .assoc = left } 
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

static void reduce(coyc_pctx_t *ctx, expression_t *expr);
/// Reduces expressions contained within values. This
/// modifies `val` in-place, which is why it requires
/// a pointer.
/// Might free `val`.
static void expr_reduce_val(coyc_pctx_t *ctx, expression_value_t *val) {
    reduce(ctx, val->expression.expression);
    if (val->expression.expression->rhs.type == none) {
        // Only the LHS matters; the op must be a terminator
        // TODO: assertion
        expression_value_t lhs = val->expression.expression->lhs;
        expression_free(val->expression.expression);
        *val = lhs;
    }
}

static bool isComptime(coyc_token_kind_t op) {
    return op == COYC_TK_OPMUL || op == COYC_TK_OPADD || op == COYC_TK_OPDIV || op == COYC_TK_OPSUB;
}

static void reduce(coyc_pctx_t *ctx, expression_t *expr) {
    if (expr->lhs.type == expression) {
        expr_reduce_val(ctx, &expr->lhs);
    }
    if (expr->rhs.type == expression) {
        expr_reduce_val(ctx, &expr->rhs);
    }
    if (expr->lhs.type == literal && expr->rhs.type == literal && isComptime(expr->op.kind)) {
        switch (expr->op.kind) {
            case COYC_TK_OPADD:
                // TODO types
                expr->lhs.literal.value.integer.value += expr->rhs.literal.value.integer.value;
                break;
            case COYC_TK_OPSUB:
                // TODO types
                expr->lhs.literal.value.integer.value -= expr->rhs.literal.value.integer.value;
                break;
            case COYC_TK_OPMUL:
                // TODO types
                expr->lhs.literal.value.integer.value *= expr->rhs.literal.value.integer.value;
                break;
            case COYC_TK_OPDIV:
                // TODO types
                if (expr->rhs.literal.value.integer.value == 0) {
                    ERROR("Division by zero!");
                }
                expr->lhs.literal.value.integer.value /= expr->rhs.literal.value.integer.value;
                break;
            default:
                ERROR("UNREACHABLE");
            }
            expr->type = expr->lhs.literal.value.integer.type;
            expr->rhs.type = none;
        }
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
        if (token.kind == COYC_TK_SCOLON) {
            return expr;
        }
        const op_t *op = find_op(token);
        if (op == NULL || op->prec < minimum_prec) {
            // Done
            return expr;
        }
        if (expr->rhs.type != none) {
            expression_t *subexpr = expr;
            expr = malloc(sizeof(expression_t));
            expr->lhs.expression.type = expression;
            expr->lhs.expression.expression = subexpr;
            expr->rhs.type = none;
        }
        expr->op = token;
        const int next_prec = op->assoc == left ? op->prec + 1 : op->prec;
        ctx->token_index += 1;
        expression_t *rhs = parse_expression(ctx, next_prec);
        if (expr->rhs.type != none) {
            ERROR("UNREACHABLE");
        }
        expr->rhs.expression.type = expression;
        expr->rhs.expression.expression = rhs;
        reduce(ctx, expr);
    }
}

// Returns true if there's no more instructions
static bool parse_statement(coyc_pctx_t *ctx, function_t *function)
{
    coyc_token_t token = ctx->tokens[ctx->token_index];
    if (token.kind == COYC_TK_EOF) {
        ERROR("Error: expected '}', found EOF!");
    }
    if (token.kind == COYC_TK_ERROR) {
        ERROR("Error in lexer while parsing function body!");
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
        if (ctx->tokens[ctx->token_index].kind == COYC_TK_SCOLON) {
            ctx->token_index += 1;
        }
        else {
            ERROR("Expected semicolon after return statement!");
        }
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
    decl.function.parameters = NULL;
    decl.function.return_type = type;
    coyc_token_t token = ctx->tokens[ctx->token_index];
    while (token.kind == COYC_TK_TYPE || token.kind == COYC_TK_COMMA) {
        coyc_token_kind_t expected = arrlenu(decl.function.parameters) == 0 ? COYC_TK_TYPE : COYC_TK_COMMA;
        if (token.kind != expected) {
            if (token.kind == COYC_TK_COMMA) {
                ERROR("Expected type in param list, found comma!");
            }
            else {
                ERROR("Expected comma to separate parameters in param list!");
            }
        }
        if (token.kind == COYC_TK_COMMA) {
            ctx->token_index += 1;
            token = ctx->tokens[ctx->token_index];
        }
        type_t type = coyc_type(token);
        if (type.primitive == invalid) {
            error_token(ctx, token, "TODO parse type %s");
        }
        ctx->token_index += 1;
        token = ctx->tokens[ctx->token_index];
        if (token.kind != COYC_TK_IDENT) {
            errorf(ctx, "Expected identifier, found %s", coyc_token_kind_tostr_DBG(token.kind));
        }
        char *name = coyc_token_read(token);
        parameter_t param;
        param.type = type;
        param.name = name;
        arrput(decl.function.parameters, param);
        ctx->token_index += 1;
        token = ctx->tokens[ctx->token_index];
    }
    if (token.kind != COYC_TK_RPAREN) {
        ERROR("Expected ')' to end function parameter list!");
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
    root->module_name = "<undefined>";

    ctx->err_msg = NULL;
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
