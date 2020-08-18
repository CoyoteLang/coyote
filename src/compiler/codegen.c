#include "codegen.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stb_ds.h"
#include "util/debug.h"
#include "util/hints.h"
#include "util/string.h"
#include "function_builder.h"

static COY_HINT_NORETURN COY_HINT_PRINTF(2, 3) void errorf(coyc_cctx_t *ctx, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char *buf = coy_vaprintf_(fmt, args);
    va_end(args);
    ctx->err_msg = buf;
    fprintf(stderr, "Codegen: %s\n", buf);
    longjmp(ctx->err_env, 255);
}

#define ERROR(msg) do { errorf(ctx, "%s at %s:%d", msg, __FILE__, __LINE__); } while (0);

static uint32_t coyc_gen_expr(coyc_cctx_t *ctx, struct coy_function_builder_ *builder, expression_t *expr);

static size_t expr_value_reg(coyc_cctx_t *ctx, struct coy_function_builder_ *builder, expression_value_t value) {
    switch (value.type) {
    case expression:
        return coyc_gen_expr(ctx, builder, value.expression.expression);
    case identifier:
        ERROR("Internal error: identifier remaining post-semalysis");
    case parameter:
        return value.parameter.index;
    default:
        COY_TODO("exprgen_value more");
    }
}


static coy_instruction_opcode_t fromToken(coyc_token_kind_t op) {
    switch (op) {
    case COYC_TK_OPMUL:
        return COY_OPCODE_MUL;
    case COYC_TK_OPADD:
        return COY_OPCODE_ADD;
    case COYC_TK_OPDIV:
        return COY_OPCODE_DIV;
    case COYC_TK_OPSUB:
        return COY_OPCODE_SUB;
    default:
        return COY_OPCODE_NOP; 
    }
}

/// Returns the SSA reg containing the final result 
static uint32_t coyc_gen_expr(coyc_cctx_t *ctx, struct coy_function_builder_ *builder, expression_t *expr) {
    size_t lhs = expr_value_reg(ctx, builder, expr->lhs);
    size_t rhs = expr_value_reg(ctx, builder, expr->rhs);

    // TODO: check types
    const coy_instruction_opcode_t op = fromToken(expr->op.kind);
    if (op == COY_OPCODE_NOP) {
        ERROR("TODO more ops")
    }
    
    uint32_t reg = coy_function_builder_op_(builder, op, COY_OPFLG_TYPE_UINT32, false);
    coy_function_builder_arg_reg_(builder, lhs);
    coy_function_builder_arg_reg_(builder, rhs);


    return reg;
}

static void coyc_gen_func(coyc_cctx_t *ctx, function_t func) {

    ctx->func = &func;
    
    struct coy_function_builder_ builder;
    coy_function_builder_init_(&builder, &func.type, 0);
    uint32_t block = coy_function_builder_block_(&builder, arrlenu(func.parameters), NULL, 0);
    coy_function_builder_useblock_(&builder, block);
    for (size_t i = 0; i < arrlenu(func.statements); i += 1) {
        statement_t statement = func.statements[i];
        switch (statement.type) {
        case return_:{
            const size_t reg = coyc_gen_expr(ctx, &builder, statement.return_.value);
            coy_function_builder_op_(&builder, COY_OPCODE_RET, COY_OPFLG_TYPE_UINT32, false);
            coy_function_builder_arg_reg_(&builder, reg);
            break;}
        default:
            errorf(ctx, "TODO: semalysis stmt type %d", statement.type);
        }
    }
    arraddn(ctx->module->functions, 1);
    coy_function_builder_finish_(&builder, &arrlast(ctx->module->functions));
}

coyc_cctx_t coyc_codegen(ast_root_t *root) {
    coyc_cctx_t ctx;
    ctx.err_msg = NULL;
    coy_module_t *module = malloc(sizeof(coy_module_t));
    module->variables = NULL;
    module->vtbl = NULL;
    module->functions = NULL;
    ctx.module = module;
    if (setjmp(ctx.err_env) == 255) {
        coyc_cg_free(ctx);
        ctx.module = NULL;
        return ctx;
    }
    for (size_t i = 0; i < arrlenu(root->decls); i+= 1) {
        decl_t decl = root->decls[i];
        if (decl.base.type == function) {
            coyc_gen_func(&ctx, decl.function);
        } else {
            errorf(&ctx, "TODO semantic analysis non-function entries");
        }
    }
    return ctx;
}

void coyc_cg_free(coyc_cctx_t ctx) {
    for (int i = 0; i < arrlen(ctx.module->functions); i += 1) {
       // TODO
    }
    arrfree(ctx.module->functions);
    free(ctx.module);
}
