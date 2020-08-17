#include "semalysis.h"
#include "bytecode.h"
#include "token.h"
#include "util/string.h"
#include "util/hints.h"

#include "stb_ds.h"

#include <stdlib.h>
#include <stdio.h>

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

static bool op_is_arithmetic(coyc_token_kind_t op) {
    return op == COYC_TK_OPMUL || op == COYC_TK_OPADD || op == COYC_TK_OPSUB || op == COYC_TK_OPDIV;
}

static void resolve_type(coyc_sctx_t *ctx, function_t func, expression_t *expr) {
    if (expr->lhs.type == expression) {
        resolve_type(ctx, func, expr->lhs.expression.expression);
    }
    if (expr->rhs.type == expression) {
        resolve_type(ctx, func, expr->rhs.expression.expression);
    }

    struct coy_typeinfo_ l, r;
    if (expr->lhs.type == parameter) {
        l = func.parameters[expr->lhs.parameter.index].type;
    }
    else if (expr->lhs.type == expression) {
        l = expr->lhs.expression.expression->type;
    }
    else {
        ERROR("Resolve type [^parameter]");
    }
    if (expr->rhs.type == parameter) {
        r = func.parameters[expr->rhs.parameter.index].type;
    }
    else if (expr->rhs.type == expression) {
        r = expr->rhs.expression.expression->type;
    }
    else {
        ERROR("Resolve type [^parameter]");
    }

    if (op_is_arithmetic(expr->op.kind)) {
        if (coy_type_eql_(l, r)) {
            expr->type = l;
        }
        else {
            ERROR("TODO");
        }
    }
    else {
        ERROR("TODO");
    }
}

static void resolve_idents(coyc_sctx_t *ctx, function_t func, expression_t *expr) {
    if (expr->lhs.type == expression) {
        resolve_idents(ctx, func, expr->lhs.expression.expression);
    }
    if (expr->rhs.type == expression) {
        resolve_idents(ctx, func, expr->rhs.expression.expression);
    }
    if (expr->lhs.type == identifier) {
        for (size_t i = 0; i < arrlenu(func.parameters); i+= 1) {
            parameter_t param = func.parameters[i];
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
        for (size_t i = 0; i < arrlenu(func.parameters); i+= 1) {
            parameter_t param = func.parameters[i];
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

static void coyc_sema_func(coyc_sctx_t *ctx, function_t func) {
    for (size_t i = 0; i < arrlenu(func.statements); i += 1) {
        statement_t statement = func.statements[i];
        switch (statement.type) {
        case return_:{
            resolve_idents(ctx, func, statement.return_.value);
            resolve_type(ctx, func, statement.return_.value);
            const struct coy_typeinfo_ returned = statement.return_.value->type;
            if (!_type_coerces(returned, func.return_type)) {
                errorf(ctx, "Returning type %s, %s expected", coy_type_str_( returned), coy_type_str_(func.return_type));
            }
            break;}
        default:
            errorf(ctx, "TODO: semalysis stmt type %d", statement.type);
        }
    }
}

char *coyc_semalysis(ast_root_t *root) {
    coyc_sctx_t *ctx = malloc(sizeof(coyc_sctx_t));
    ctx->err_msg = NULL;
    if (setjmp(ctx->err_env) == 255) {
        char *msg = ctx->err_msg;
        free(ctx);
        return msg;
    }
    for (size_t i = 0; i < arrlenu(root->decls); i+= 1) {
        decl_t decl = root->decls[i];
        if (decl.base.type == function) {
            coyc_sema_func(ctx, decl.function);
        } else {
            errorf(ctx, "TODO semantic analysis non-function entries");
        }
    }

    free(ctx);
    return NULL;
}
