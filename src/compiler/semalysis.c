#include "semalysis.h"
#include "bytecode.h"
#include "token.h"
#include "util/debug.h"
#include "util/string.h"
#include "util/hints.h"

#include "stb_ds.h"

#include <stdlib.h>
#include <stdio.h>

typedef struct {
    jmp_buf err_env;
    char *err_msg;
    ast_root_t *root;
    function_t *func;
    statement_t *stmt;
} coyc_sctx_t;

static COY_HINT_NORETURN COY_HINT_PRINTF(2, 3) void errorf(coyc_sctx_t *ctx, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char *buf = coy_vaprintf_(fmt, args);
    va_end(args);
    ctx->err_msg = buf;
    fprintf(stderr, "Semantic analysis: %s\n", buf);
    longjmp(ctx->err_env, 255);
}

#define ERROR(msg) do { errorf(ctx, "%s at %s:%d", msg, __FILE__, __LINE__); } while (0);

bool coy_type_eql_(struct coy_typeinfo_ a, struct coy_typeinfo_ b) {
    if (a.category != b.category) {
        return false;
    }
    switch (a.category) {
        case COY_TYPEINFO_CAT_INTERNAL_:
            return false;
        case COY_TYPEINFO_CAT_INTEGER_:
            return a.u.integer.is_signed == b.u.integer.is_signed && a.u.integer.width == b.u.integer.width;
        default:
            return false;
    }
}

bool _type_coerces(struct coy_typeinfo_ to, struct coy_typeinfo_ from) {
    if (coy_type_eql_(to, from))
        return true;
    if (to.category != from.category)
        return false;
    switch (to.category) {
        default:
            return false;
    }
}

char *coy_type_str_(struct coy_typeinfo_ a) {
    switch (a.category) {
        case COY_TYPEINFO_CAT_INTEGER_:
            return coy_aprintf_("%sint%u", a.u.integer.is_signed ? " " : "u", a.u.integer.width);
        default:
            break;
    }
    return coy_strdup_("none", 4);
}

static bool op_is_arithmetic(op_type_t op) {
    return op == OP_MUL || op == OP_ADD || op == OP_SUB || op == OP_DIV;
}

bool op_is_unary(op_type_t op) {
    return op == OP_CALL || op == OP_NONE;
}

static function_t *lookup_func(coyc_sctx_t *ctx, char *name) {
    ast_root_t *root = ctx->root;
    for (size_t i = 0; i < arrlenu(root->decls); i += 1) {
        decl_t *decl = &root->decls[i];
        if (decl->base.type == function) {
            if (!strcmp(name, decl->base.name)) {
                return &decl->function;
            }
        }
    }
    return NULL;
}

static struct coy_typeinfo_ resolve_expr_val_type(coyc_sctx_t *ctx, expression_value_t value, expression_t *parent) {
    switch (value.type) {
    case parameter:
        return ctx->func->parameters[value.parameter.index].type;
    case expression:
        return value.expression.expression->type;
        break;
    case call:{
        function_t *func = lookup_func(ctx, value.call.name);
        if (!func) {
            errorf(ctx, "Identifier not found: '%s'\n", value.call.name);
        }
        return func->return_type;
        }
    case literal:
        if (parent->op == OP_NONE) {
            // Raw literal; value depends on statement
            if (ctx->stmt->type == return_) {
                COY_ASSERT(parent == ctx->stmt->expr.value);
            }
            if (ctx->func->return_type.category != COY_TYPEINFO_CAT_INTEGER_) {
                ERROR("Returning integer literal from function not returning integer!");
            }
            return ctx->func->return_type;
        }
        ERROR("TODO: resolve subexpr literal type");
        break;
    case none:
        ERROR("UNREACHABLE");
    default:
        ERROR("Resolve type [^parameter]");
    };

}

static void resolve_expr_type(coyc_sctx_t *ctx, expression_t *expr) {
    if (expr->lhs.type == expression) {
        resolve_expr_type(ctx,expr->lhs.expression.expression);
    }
    if (expr->rhs.type == expression) {
        resolve_expr_type(ctx, expr->rhs.expression.expression);
    }

    struct coy_typeinfo_ l = resolve_expr_val_type(ctx, expr->lhs, expr);
    if (op_is_unary(expr->op)) {
        expr->type = l;
        return;
    }
    else {
        struct coy_typeinfo_ r = resolve_expr_val_type(ctx, expr->rhs, expr);

        if (op_is_arithmetic(expr->op)) {
            if (coy_type_eql_(l, r)) {
                expr->type = l;
                return;
            }
        }
    }
    ERROR("TODO");
}

static void resolve_idents(coyc_sctx_t *ctx, expression_t *expr) {
    if (expr->lhs.type == expression) {
        resolve_idents(ctx, expr->lhs.expression.expression);
    }
    if (expr->rhs.type == expression) {
        resolve_idents(ctx, expr->rhs.expression.expression);
    }
    if (expr->lhs.type == identifier) {
        for (size_t i = 0; i < arrlenu(ctx->func->parameters); i+= 1) {
            parameter_t param = ctx->func->parameters[i];
            if (!strcmp(param.name, expr->lhs.identifier.name)) {
                // name is owned by identifier, so free it.
                free(expr->lhs.identifier.name);
                expr->lhs.type = parameter;
                expr->lhs.parameter.index = i;
                break;
            }
        }
        if (expr->lhs.type == identifier) {
            errorf(ctx, "identifier not found: %s", expr->lhs.identifier.name);
        }
    }
    if (expr->rhs.type == identifier) {
        for (size_t i = 0; i < arrlenu(ctx->func->parameters); i+= 1) {
            parameter_t param = ctx->func->parameters[i];
            if (!strcmp(param.name, expr->rhs.identifier.name)) {
                // name is owned by identifier, so free it.
                free(expr->rhs.identifier.name);
                expr->rhs.type = parameter;
                expr->rhs.parameter.index = i;
                break;
            }
        }
        if (expr->rhs.type == identifier) {
            errorf(ctx, "identifier not found: %s", expr->rhs.identifier.name);
        }
    }
}

static void coyc_sema_block(coyc_sctx_t *ctx, block_t block) {
    for (size_t j = 0; j < arrlenu(block.statements); j += 1) {
        statement_t statement = block.statements[j];
        ctx->stmt = &statement;
        switch (statement.type) {
        case return_:
        case expr:{
            resolve_idents(ctx, statement.expr.value);
            resolve_expr_type(ctx, statement.expr.value);
            const struct coy_typeinfo_ returned = statement.expr.value->type;
            if (!_type_coerces(returned, ctx->func->return_type)) {
                errorf(ctx, "Returning type %s, %s expected", coy_type_str_( returned), coy_type_str_(ctx->func->return_type));
            }
            break;}
        case conditional:
            resolve_idents(ctx, statement.conditional.condition);
            resolve_expr_type(ctx, statement.conditional.condition);
            const struct coy_typeinfo_ cond_type = statement.conditional.condition->type;
            if (cond_type.category != COY_TYPEINFO_CAT_INTEGER_ || cond_type.u.integer.is_signed) {
                errorf(ctx, "Expected condition type to coerce to unsigned integer!");
            }
            // DON'T sema the cond's blocks, they're owned by the function as well and will be handled as such
            break;
        default:
            errorf(ctx, "TODO: semalysis stmt type %d", statement.type);
        }
    }
}

static void coyc_sema_func(coyc_sctx_t *ctx) {
    ctx->func->type.category = COY_TYPEINFO_CAT_FUNCTION_;
    ctx->func->type.u.function.rtype = &ctx->func->return_type;
    const size_t params = arrlenu(ctx->func->parameters);
    ctx->func->type.u.function.ptypes = malloc(sizeof(struct coy_typeinfo_*) * (params + 1));
    ctx->func->type.u.function.ptypes[params] = NULL;
    for (size_t i = 0; i < params; i += 1) {
        ctx->func->type.u.function.ptypes[i] = &ctx->func->parameters[i].type;
    }
    for (size_t i = 0; i < arrlenu(ctx->func->blocks); i += 1) {
        coyc_sema_block(ctx, ctx->func->blocks[i]);
    }
}

char *coyc_semalysis(ast_root_t *root) {
    coyc_sctx_t *ctx = malloc(sizeof(coyc_sctx_t));
    ctx->err_msg = NULL;
    ctx->root = root;
    if (setjmp(ctx->err_env) == 255) {
        char *msg = ctx->err_msg;
        free(ctx);
        return msg;
    }
    for (size_t i = 0; i < arrlenu(root->decls); i+= 1) {
        decl_t *decl = root->decls + i;
        if (decl->base.type == function) {
            ctx->func = &decl->function;
            coyc_sema_func(ctx);
        } else {
            errorf(ctx, "TODO semantic analysis non-function entries");
        }
    }

    free(ctx);
    return NULL;
}
