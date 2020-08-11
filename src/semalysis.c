#include "semalysis.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stb_ds.h"
#include "util/hints.h"
#include "util/string.h"

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

static size_t lookup_reg(coyc_sctx_t *ctx, char *ident) {
    for (size_t i = 0; i < arrlenu(ctx->indices); i += 1) {
        if (!strcmp(ctx->indices[i], ident)) {
            return ctx->regs[i];
        }
    }
    return -1;
}

static size_t coyc_sema_exprgen(coyc_sctx_t *ctx, coy_instruction_t **instrs, expression_t *expr);

static size_t expr_value_reg(coyc_sctx_t *ctx, coy_instruction_t **instrs, expression_value_t value) {
    switch (value.type) {
    case expression:
        return coyc_sema_exprgen(ctx, instrs, value.expression.expression);
    case identifier:{
        size_t reg = lookup_reg(ctx, value.identifier.name);
        if (reg == (size_t)-1) {
            errorf(ctx, "Identifier '%s' does not exist!", value.identifier.name);
        }
        return reg;}
    default:
        errorf(ctx, "TODO exprgen_value %d", value.type);
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
static size_t coyc_sema_exprgen(coyc_sctx_t *ctx, coy_instruction_t **instrs, expression_t *expr) {
    size_t lhs = expr_value_reg(ctx, instrs, expr->lhs);
    size_t rhs = expr_value_reg(ctx, instrs, expr->rhs);

// TODO: check types

    coy_instruction_t insn = { .op = {fromToken(expr->op.kind), COY_OPFLG_TYPE_UINT32, 0, 2} };
    if (insn.op.code == COY_OPCODE_NOP) {
        ERROR("TODO more ops")
    }
    arrput(*instrs, insn);
    coy_instruction_t larg = {.arg = {lhs, 0}}, rarg = {.arg = {rhs, 0}};
    arrput(*instrs, larg);
    arrput(*instrs, rarg);
    return arrlenu(*instrs);
}

// TODO: this is a PRIVATE API!
void coy_function_update_maxregs_(struct coy_function_* func);
static void coyc_sema_func(coyc_sctx_t *ctx, function_t func) {
    size_t scope_base = arrlenu(ctx->scope.entries);

    ctx->indices = NULL;
    ctx->regs = NULL;

    for (size_t i = 0; i < arrlenu(func.parameters); i += 1) {
        scope_entry_t entry;
        entry.param.type = param;
        entry.param.param = func.parameters[i];
        arrput(ctx->scope.entries, entry);
        arrput(ctx->indices, entry.param.param.name);
        arrput(ctx->regs, i);
    }

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
            const size_t reg = coyc_sema_exprgen(ctx, &gen_func.u.coy.instrs, statement.return_.value);
            const coy_instruction_t ret = {.op = {COY_OPCODE_RET, COY_OPFLG_TYPE_UINT32, 0, 1}};
            arrput(gen_func.u.coy.instrs, ret);
            const coy_instruction_t arg = {.arg = {reg, 0}};
            arrput(gen_func.u.coy.instrs, arg);
            break;}
        default:
            errorf(ctx, "TODO: semalysis stmt type %d", statement.type);
        }
    }

    size_t new_base = arrlenu(ctx->scope.entries);
    if (new_base != scope_base) {
        arrdeln(ctx->scope.entries, scope_base, new_base - scope_base);
    }

    coy_function_update_maxregs_(&gen_func);
    arrput(ctx->module->functions, gen_func);

}

coyc_sctx_t *coyc_semalysis(ast_root_t *root) {
    coyc_sctx_t *ctx = malloc(sizeof(coyc_sctx_t));
    ctx->scope.entries = NULL;
    ctx->err_msg = NULL;
    coy_module_t *module = malloc(sizeof(coy_module_t));
    module->variables = NULL;
    module->vtbl = NULL;
    module->functions = NULL;
    ctx->module = module;
    if (setjmp(ctx->err_env) == 255) {
        coy_module_free(module);
        ctx->module = NULL;
        return ctx;
    }
    for (size_t i = 0; i < arrlenu(root->decls); i+= 1) {
        decl_t decl = root->decls[i];
        if (decl.base.type == function) {
            coyc_sema_func(ctx, decl.function);
        } else {
            errorf(ctx, "TODO semantic analysis non-function entries");
        }
    }
    return ctx;
}

void coy_module_free(coy_module_t *module) {
    free(module);
}