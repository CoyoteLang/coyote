/**
 Implementation notes

 This is currently not error-tolerant, at all. Post-jam, making the parser fully
tolerant is extremely important, and a high-priority TODO.
 */

#include "ast.h"
#include "util/string.h"
#include "util/hints.h"
#include "util/debug.h"
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

#define ERROR(msg) do { errorf(ctx, "%s", msg); } while (0);

static struct coy_typeinfo_ coyc_type(coyc_token_t token) {
    struct coy_typeinfo_ type;
    type.category = COY_TYPEINFO_CAT_INTERNAL_;
    if (strncmp(token.ptr, "int", token.len) == 0) {
        type.category = COY_TYPEINFO_CAT_INTEGER_;
        type.u.integer.is_signed = true;
        type.u.integer.width = 32;
    }
    if (!strncmp(token.ptr, "uint", token.len) || !strncmp(token.ptr, "u32", token.len)) {
        type.category = COY_TYPEINFO_CAT_INTEGER_;
        type.u.integer.is_signed = false;
        type.u.integer.width = 32;
    }
    if (type.category == COY_TYPEINFO_CAT_INTERNAL_) {
        char *tok = coyc_token_read(token);
        printf("Parse type '%s'", tok);
        __builtin_trap();
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

static expression_value_t compute_atom(coyc_pctx_t *ctx) {
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
        // TODO: error checking
        sscanf(token.ptr, "%" SCNu32, &val.literal.value.integer.value);
        ctx->token_index += 1;
        return val;
    }
    if (token.kind == COYC_TK_IDENT) {
        if (ctx->tokens[ctx->token_index + 1].kind == COYC_TK_LPAREN) {
            // Function call
            char *name = coyc_token_read(token);
            fprintf(stderr, "Parsed call to function %s\n", name);
            ctx->token_index += 2;
            token = ctx->tokens[ctx->token_index];
            expression_value_t value;
            value.call.type = call;
            value.call.name = name;
            value.call.arguments = NULL;
            if (token.kind != COYC_TK_RPAREN) do {
                // TODO: have parse_expression return expression_values instead
                expression_value_t arg_val;
                arg_val.expression.type = expression;
                arg_val.expression.expression = parse_expression(ctx, 1);
                arrput(value.call.arguments, arg_val);
            } while (ctx->tokens[ctx->token_index].kind == COYC_TK_COMMA);
            if (ctx->tokens[ctx->token_index].kind != COYC_TK_RPAREN) {
                ERROR("Exepected ')' to close function call!");
            }
            ctx->token_index += 1;
            return value;
        }
        else {
            expression_value_t val;
            val.identifier.type = identifier;
            val.identifier.name = coyc_token_read(token);
            ctx->token_index += 1;
            return val;
        }
    }
    else {
        COY_TODO("More atoms");
    }
    // Not yet used.
}

typedef struct {
    coyc_token_kind_t token;
    unsigned prec;
    enum {left,right} assoc;
} op_t;

op_t ops[6] = {
   { .token = COYC_TK_OPADD, .prec = 1, .assoc = left },
   { .token = COYC_TK_OPSUB, .prec = 1, .assoc = left },
   { .token = COYC_TK_OPDIV, .prec = 2, .assoc = left },
   { .token = COYC_TK_OPMUL, .prec = 2, .assoc = left },
   { .token = COYC_TK_CMPLT, .prec = 3, .assoc = left },
   { .token = COYC_TK_CMPGT, .prec = 3, .assoc = left },
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

static bool isComptime(op_type_t op) {
    return op == OP_DIV || op == OP_ADD || op == OP_SUB || op == OP_MUL;
}

static void reduce(coyc_pctx_t *ctx, expression_t *expr) {
    if (expr->lhs.type == expression) {
        expr_reduce_val(ctx, &expr->lhs);
    }
    if (expr->rhs.type == expression) {
        expr_reduce_val(ctx, &expr->rhs);
    }
    if (expr->lhs.type == literal && expr->rhs.type == literal && isComptime(expr->op)) {
        switch (expr->op) {
            case OP_ADD:
                // TODO types
                expr->lhs.literal.value.integer.value += expr->rhs.literal.value.integer.value;
                break;
            case OP_SUB:
                // TODO types
                expr->lhs.literal.value.integer.value -= expr->rhs.literal.value.integer.value;
                break;
            case OP_MUL:
                // TODO types
                expr->lhs.literal.value.integer.value *= expr->rhs.literal.value.integer.value;
                break;
            case OP_DIV:
                // TODO types
                if (expr->rhs.literal.value.integer.value == 0) {
                    ERROR("Division by zero!");
                }
                expr->lhs.literal.value.integer.value /= expr->rhs.literal.value.integer.value;
                break;
            default:
                ERROR("UNREACHABLE");
            }
            // This should be resolved by sema into an appropriate integral type
            expr->type.category = COY_TYPEINFO_CAT_INTERNAL_;
            expr->rhs.type = none;
        }
}

static op_type_t op_type_from(coyc_token_kind_t kind) {
    switch (kind) {
    case COYC_TK_OPADD:
        return OP_ADD;
    case COYC_TK_OPMUL:
        return OP_MUL;
    case COYC_TK_OPDIV:
        return OP_DIV;
    case COYC_TK_OPSUB:
        return OP_SUB;
    case COYC_TK_CMPGT:
        return OP_CMPGT;
    case COYC_TK_CMPLT:
        return OP_CMPLT;
    default:
        COY_TODO("More op_type_from");
    }
}

static bool is_unary(expression_value_type_t type) {
    return type == call;
}

static expression_t *parse_expression(coyc_pctx_t *ctx, unsigned int minimum_prec) {
    expression_t *expr = malloc(sizeof(expression_t));
    expr->lhs = compute_atom(ctx);
    expr->rhs.type = none;
    expr->type.category = COY_TYPEINFO_CAT_INTERNAL_;
    expr->op = OP_NONE;
    if (is_unary(expr->lhs.type)) {
        if (expr->lhs.type == call) {
            expr->op = OP_CALL;
        }
    }
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
            expr->type.category = COY_TYPEINFO_CAT_INTERNAL_;
        }
        expr->op = op_type_from(token.kind);
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
    statement_t stmt;
    coyc_token_t token = ctx->tokens[ctx->token_index];
    switch (token.kind) {
    case COYC_TK_EOF:
        ERROR("Error: expected '}', found EOF!");
    case COYC_TK_ERROR:
        ERROR("Error in lexer while parsing function body!");
    case COYC_TK_RBRACE:
        ctx->token_index += 1;
        return true;
    case COYC_TK_RETURN:
        ctx->token_index += 1;
        token = ctx->tokens[ctx->token_index];
        stmt.expr.type = return_;
        stmt.expr.value = parse_expression(ctx, 1);
        token = ctx->tokens[ctx->token_index];
        if (token.kind == COYC_TK_SCOLON) {
            ctx->token_index += 1;
        }
        else {
            printf("Found %s\n", coyc_token_kind_tostr_DBG(token.kind));
            ERROR("Expected semicolon after return statement!");
        }
        break;
    case COYC_TK_IF:
        if (ctx->tokens[ctx->token_index + 1].kind != COYC_TK_LPAREN) {
            ERROR("Expected '(' after `if`!");
        }
        ctx->token_index += 2;
        stmt.type = conditional;
        stmt.conditional.condition = parse_expression(ctx, 1);
        stmt.conditional.false_block = arraddnindex(function->blocks, 2);
        stmt.conditional.true_block = stmt.conditional.false_block + 1;
        function->blocks[stmt.conditional.false_block].statements = NULL;
        function->blocks[stmt.conditional.true_block].statements = NULL;
        function->blocks[stmt.conditional.false_block].parameters = NULL;
        function->blocks[stmt.conditional.true_block].parameters = NULL;
        if (ctx->tokens[ctx->token_index].kind != COYC_TK_RPAREN) {
            ERROR("Expected ')' to close conditional expression!");
        }
        ctx->token_index += 1;
        if (ctx->tokens[ctx->token_index].kind == COYC_TK_LBRACE) {
            ERROR("Conditional blocks not implemented yet :P");
        }
        arrput(function->active_block->statements, stmt);
        function->active_block = &function->blocks[stmt.conditional.true_block];
        if (parse_statement(ctx, function)) {
            ERROR("Expected true-statement for conditional before '}'!");
        }
        function->active_block = &function->blocks[stmt.conditional.false_block];
        return false;
    case COYC_TK_IDENT:
        if (ctx->tokens[ctx->token_index + 1].kind == COYC_TK_LPAREN) {
            stmt.expr.type = expr;
            stmt.expr.value = parse_expression(ctx, 1);
            if (ctx->tokens[ctx->token_index].kind != COYC_TK_SCOLON) {
                ERROR("Expected semicolon after expression!");
            }
            ctx->token_index += 1;
        }
        else {
            ERROR("TODO non-function-call pattern IDENT");
        }
        break;
    default:
        errorf(ctx, "TODO parse statement %s", coyc_token_kind_tostr_DBG(token.kind));
    }
    arrput(function->active_block->statements, stmt);
    return false;
}

/// token_index must mark the first token after the LPAREN
static void parse_function(coyc_pctx_t *ctx, struct coy_typeinfo_ return_type, char *ident)
{
    decl_t decl;
    decl.function.base.type = function;
    decl.function.base.name = ident;
    decl.function.active_block = NULL;
    decl.function.blocks = NULL;
    arraddn(decl.function.blocks, 1);
    decl.function.blocks[0].statements = NULL;
    decl.function.blocks[0].parameters = NULL;
    decl.function.active_block = &decl.function.blocks[0];
    decl.function.return_type = return_type;
    decl.function.type.category = COY_TYPEINFO_CAT_INTERNAL_;
    coyc_token_t token = ctx->tokens[ctx->token_index];
    while (token.kind == COYC_TK_TYPE || token.kind == COYC_TK_COMMA) {
        coyc_token_kind_t expected = arrlenu(decl.function.blocks[0].parameters) == 0 ? COYC_TK_TYPE : COYC_TK_COMMA;
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
        struct coy_typeinfo_ type = coyc_type(token);
        ctx->token_index += 1;
        token = ctx->tokens[ctx->token_index];
        if (token.kind != COYC_TK_IDENT) {
            errorf(ctx, "Expected identifier, found %s", coyc_token_kind_tostr_DBG(token.kind));
        }
        char *name = coyc_token_read(token);
        parameter_t param;
        param.type = type;
        param.name = name;
        arrput(decl.function.blocks[0].parameters, param);
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
    switch (token.kind) {
    case COYC_TK_TYPE:{
        const struct coy_typeinfo_ type = coyc_type(token);
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
        break;}
    case COYC_TK_MODULE:
        ERROR("Duplicate module statement found!");
    default:
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

    if (setjmp(ctx->err_env) == 255)
    {
        // Error
        return;
    }

    for(;;){
        coyc_token_t token = coyc_lexer_next(lexer, COYC_LEXER_CATEGORY_PARSER);
        if(token.kind == COYC_TK_EOF)
            break;
        if (token.kind == COYC_TK_ERROR)
            ERROR("Lexer error");
        arrput(ctx->tokens, token);
    }

    if (arrlen(ctx->tokens) == 0) {
        coyc_tree_free(ctx);
        return;
    }

    parse_module(ctx);
    while (ctx->token_index < arrlenu(ctx->tokens)) {
        parse_decl(ctx);
    }

}

static void coyc_expr_free(expression_t *expr);
static void coyc_exprv_free(expression_value_t val) {
    if (val.type == expression) {
        coyc_expr_free(val.expression.expression);
    }
    if (val.type == identifier) {
        free(val.identifier.name);
    }
}

static void coyc_expr_free(expression_t *expr) {
    coyc_exprv_free(expr->lhs);
    coyc_exprv_free(expr->rhs);
    free(expr);
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
                    if (decl->function.blocks) {
                        for (int j = 0; j < arrlen(decl->function.blocks); j += 1) {
                            block_t block = decl->function.blocks[j];
                            for (int k = 0; k < arrlen(block.statements); k += 1) {
                                statement_t statement = block.statements[k];
                                switch (statement.type) {
                                case return_:
                                case expr:
                                    coyc_expr_free(statement.expr.value);
                                    break;
                                case conditional:
                                    coyc_expr_free(statement.conditional.condition);
                                    // The true and false blocks are owned by the function; don't clean them here.
                                    break;
                                default:
                                    COY_TODO("cleanup more statements");
                                }
                            }
                            arrfree(block.statements);
                            arrfree(block.parameters);
                        }
                        arrfree(decl->function.blocks);
                    }
                }
            }
            arrfree(ctx->root->decls);
        }
    }
}
