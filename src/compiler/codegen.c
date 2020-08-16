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

static size_t coyc_gen_expr(coyc_cctx_t *ctx, coy_instruction_t **instrs, expression_t *expr);

static size_t expr_value_reg(coyc_cctx_t *ctx, coy_instruction_t **instrs, expression_value_t value) {
    switch (value.type) {
    case expression:
        return coyc_gen_expr(ctx, instrs, value.expression.expression);
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
static size_t coyc_gen_expr(coyc_cctx_t *ctx, coy_instruction_t **instrs, expression_t *expr) {
    size_t lhs = expr_value_reg(ctx, instrs, expr->lhs);
    size_t rhs = expr_value_reg(ctx, instrs, expr->rhs);

    // TODO: check types

    coy_instruction_t insn = { .op = {fromToken(expr->op.kind), COY_OPFLG_TYPE_UINT32, 0, 2} };
    if (insn.op.code == COY_OPCODE_NOP) {
        ERROR("TODO more ops")
    }
    size_t reg = arrlen(*instrs) + arrlen(ctx->func->parameters);
    arrput(*instrs, insn);
    coy_instruction_t larg = {.arg = {lhs, 0}}, rarg = {.arg = {rhs, 0}};
    arrput(*instrs, larg);
    arrput(*instrs, rarg);
    return reg;
}

// TODO: this is a PRIVATE API!
void coy_function_update_maxregs_(struct coy_compat_function_* func);
static void coyc_gen_func(coyc_cctx_t *ctx, function_t func) {

    ctx->func = &func;

    coy_function_t gen_func;
    gen_func.blocks = NULL;
    gen_func.u.coy.instrs = NULL;
    coy_function_block_t block;
    block.offset = 0;
    block.nparams = arrlenu(func.parameters);
    arrput(gen_func.blocks, block);

    for (size_t i = 0; i < arrlenu(func.statements); i += 1) {
        statement_t statement = func.statements[i];
        switch (statement.type) {
        case return_:{
            const size_t reg = coyc_gen_expr(ctx, &gen_func.u.coy.instrs, statement.return_.value);
            const coy_instruction_t ret = {.op = {COY_OPCODE_RET, COY_OPFLG_TYPE_UINT32, 0, 1}};
            arrput(gen_func.u.coy.instrs, ret);
            const coy_instruction_t arg = {.arg = {reg, 0}};
            arrput(gen_func.u.coy.instrs, arg);
            break;}
        default:
            errorf(ctx, "TODO: semalysis stmt type %d", statement.type);
        }
    }

    coy_function_update_maxregs_(&gen_func);
    arrput(ctx->module->functions, gen_func);

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

        //coy_module_free(module);
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
        coy_function_t func = ctx.module->functions[i];
        arrfree(func.blocks);
        arrfree(func.u.coy.instrs);
    }
    arrfree(ctx.module->functions);
    free(ctx.module);
}
