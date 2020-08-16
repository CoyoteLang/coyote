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

bool _type_eql(type_t a, type_t b) {
    if (a.type == no_type || b.type == no_type) {
        return false;
    }
    return a.primitive.primitive == b.primitive.primitive;
}

bool _type_coerces(type_t to, type_t from) {
    if (_type_eql(to, from)) return true;
    if (to.type == primitive && from.type == primitive) {
        if (to.primitive.primitive == int_literal) {
            return true;
        }
    }
    return false;
}

char *_type_str_(type_t a) {
    if (a.primitive.primitive == int_literal) {
        return coy_strdup_("int_literal", 11);
    }
    if (a.primitive.primitive == _int) {
        return coy_strdup_("int", 3);
    }
    else if (a.primitive.primitive == uint) {
        return coy_strdup_("uint", 4);
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

    type_t l, r;
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
        if (_type_eql(l, r)) {
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
            const type_t returned = statement.return_.value->type;
            if (!_type_coerces(returned, func.return_type)) {
                errorf(ctx, "Returning type %s, %s expected", _type_str_( returned), _type_str_(func.return_type));
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
