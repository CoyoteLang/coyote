#include "codegen.h"

#include <inttypes.h>
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

/// Returns the value within a register, -1 if none is used (e.g. for immediates), or -2 if there is no value.
static int64_t expr_value_reg(coyc_cctx_t *ctx, struct coy_function_builder_ *builder, expression_value_t value) {
    switch (value.type) {
    case expression:
        return coyc_gen_expr(ctx, builder, value.expression.expression);
    case identifier:
        errorf(ctx, "Internal error: identifier remaining post-sema: '%s'", value.identifier.name);
    case parameter:
        return value.parameter.index;
    case call:{
        const size_t arg_count = arrlenu(value.call.arguments);
        int64_t *regs = malloc(arg_count * sizeof(int64_t));
        for (size_t i = 0; i < arg_count; i += 1) {
            regs[i] = expr_value_reg(ctx, builder, value.call.arguments[i]);
            if (regs[i] == -1) {
                COY_TODO("args outside registers");
            }
        }
        size_t size = snprintf(NULL, 0, "%s;%s", ctx->module->name, value.call.name);
        char *buf = malloc(size + 1);
        sprintf(buf, "%s;%s", ctx->module->name, value.call.name);
        printf("Generating call to '%s'\n", buf);
        uint32_t reg = coy_function_builder_op_(builder, COY_OPCODE_CALL, 0, false);
        coy_function_builder_arg_const_sym_(builder, buf);
        for (size_t i = 0; i < arg_count; i += 1) {
            coy_function_builder_arg_reg_(builder, regs[i]);
        }
        return reg;}
    case literal:
        return -1;
    case none:
        return -2;
    default:
        COY_TODO("exprgen_value more");
    }
}

static coy_instruction_opcode_t fromToken(op_type_t op) {
    switch (op) {
    case OP_MUL:
        return COY_OPCODE_MUL;
    case OP_ADD:
        return COY_OPCODE_ADD;
    case OP_DIV:
        return COY_OPCODE_DIV;
    case OP_SUB:
        return COY_OPCODE_SUB;
    case OP_CALL:
        return COY_OPCODE_CALL;
    default:
        return COY_OPCODE_NOP; 
    }
}

/// Returns the SSA reg containing the final result 
static uint32_t coyc_gen_expr(coyc_cctx_t *ctx, struct coy_function_builder_ *builder, expression_t *expr) {
    int64_t lhs = expr_value_reg(ctx, builder, expr->lhs);
    int64_t rhs = expr_value_reg(ctx, builder, expr->rhs);

    // TODO: check types
    const coy_instruction_opcode_t op = fromToken(expr->op);
    if (op == COY_OPCODE_NOP && expr->op != OP_NONE) {
        ERROR("TODO more ops");
    }
    if (op == COY_OPCODE_NOP) {
        return -1;
    }
    
    uint32_t reg = coy_function_builder_op_(builder, op, COY_OPFLG_TYPE_UINT32, false);
    if (lhs == -1)
        coy_function_builder_arg_const_val_(builder, (union coy_register_){.u32=expr->lhs.literal.value.integer.value});
    else if (lhs != -2)
        coy_function_builder_arg_reg_(builder, lhs);
    if (rhs == -1)
        coy_function_builder_arg_const_val_(builder, (union coy_register_){.u32=expr->rhs.literal.value.integer.value});
    else if (rhs != -2)
        coy_function_builder_arg_reg_(builder, rhs);

    return reg;
}

static struct coy_typeinfo_ *value_type(coyc_cctx_t *ctx, expression_t *expr, expression_value_t *val) {
    switch (val->type) {
        case parameter:
            return &ctx->block->parameters[val->parameter.index].type;
        case literal:
            switch (expr->op){
                case OP_CMPGT:
                case OP_CMPLT:{
                    // Check the other side
                    expression_value_t other = val == &expr->lhs ? expr->rhs : expr->lhs;
                    switch (other.type) {
                    case identifier:
                        ERROR("UNREACHABLE");
                        break;
                    case parameter:
                        return &ctx->block->parameters[other.parameter.index].type;
                    default:
                        COY_TODO("");
                    }
                    break;}
                default:
                    ERROR("TODO: resolve subexpr literal type");
            }
            break;
        default:
            ERROR("TODO");
    }
}

static uint8_t conditional_flag(coyc_cctx_t *ctx, expression_t *expr) {
    struct coy_typeinfo_ *l = value_type(ctx, expr, &expr->lhs);
    struct coy_typeinfo_ *r = value_type(ctx, expr, &expr->rhs);
    const uint8_t op_flag = expr->op == OP_CMPLT ? COY_OPFLG_CMP_LT :
        0;
    if (op_flag == 0) {
        ERROR("TODO");
    }
    if (l == r) {
        if (l->category == COY_TYPEINFO_CAT_INTEGER_) {
            const uint8_t sign_flag = l->u.integer.is_signed ? COY_OPFLG_SIGNED : COY_OPFLG_UNSIGNED;
            const uint8_t bits_flag = l->u.integer.width == 32 ? COY_OPFLG_32BIT :
                l->u.integer.width == 64 ? COY_OPFLG_64BIT :
                l->u.integer.width == 16 ? COY_OPFLG_16BIT :
                l->u.integer.width == 8 ? COY_OPFLG_8BIT : 0x40;
            if (bits_flag == 0x40) {
                ERROR("Unsupport bit width for conditional");
            }
            return bits_flag | sign_flag | op_flag;
        }
    };
    __builtin_trap();
    return 0;
}

static void coy_cg_pshift(coyc_cctx_t *ctx, struct coy_function_builder_ *builder, parameter_t *oparams, parameter_t *nparams) {
    for (size_t i = 0; i < arrlenu(nparams); i += 1) {
        bool found = false;
        for (size_t j = 0; j < arrlenu(oparams); j += 1) {
            if (!strcmp(oparams[j].name, nparams[i].name)) {
                printf("Mapping '%s' from %lu to %lu\n", nparams[i].name, j, i);
                coy_function_builder_arg_reg_(builder, j);
                found = true;
                break;
            }
        }
        if (!found) {
            ERROR("Ident not found in pshift");
        }
    }
}

static void coyc_gen_func(coyc_cctx_t *ctx, function_t func) {

    ctx->func = &func;
    
    struct coy_function_builder_ builder;
    coy_function_builder_init_(&builder, &func.type, 0);
    for (size_t i = 0; i < arrlenu(func.blocks); i += 1) {
        printf("Codegenning block %lu\n", i);
        ctx->block = &func.blocks[i];
        coy_function_builder_block_(&builder, arrlenu(ctx->block->parameters), NULL, 0);
        for (size_t j = 0; j < arrlenu(ctx->block->statements); j += 1) {
            statement_t statement = ctx->block->statements[j];
            switch (statement.type) {
            case return_:{
                printf("Return\n");
                const uint32_t reg = coyc_gen_expr(ctx, &builder, statement.expr.value);
                coy_function_builder_op_(&builder, COY_OPCODE_RET, COY_OPFLG_TYPE_UINT32, false);
                if (reg == (uint32_t)-1)
                    coy_function_builder_arg_const_val_(&builder, (union coy_register_){.u32=statement.expr.value->lhs.literal.value.integer.value});
                else
                    coy_function_builder_arg_reg_(&builder, reg);
                break;}
            case conditional:
                printf("Condbr\n");
                switch (statement.conditional.condition->op) {
                case OP_CMPLT:{
                    int64_t lhs = expr_value_reg(ctx, &builder, statement.conditional.condition->lhs);
                    int64_t rhs = expr_value_reg(ctx, &builder, statement.conditional.condition->rhs);
                    coy_function_builder_op_(&builder, COY_OPCODE_JMPC, conditional_flag(ctx, statement.conditional.condition), 0);
                    if (lhs == -1)
                        coy_function_builder_arg_const_val_(&builder, (union coy_register_){.u32=statement.conditional.condition->lhs.literal.value.integer.value});
                    else if (lhs != -2)
                        coy_function_builder_arg_reg_(&builder, lhs);
                    else
                        ERROR("UNREACHABLE");
                    if (rhs == -1)
                        coy_function_builder_arg_const_val_(&builder, (union coy_register_){.u32=statement.conditional.condition->rhs.literal.value.integer.value});
                    else if (rhs != -2)
                        coy_function_builder_arg_reg_(&builder, rhs);
                    else
                        ERROR("UNREACHABLE");
                    coy_function_builder_arg_imm_(&builder, statement.conditional.true_block);
                    coy_function_builder_arg_imm_(&builder, statement.conditional.false_block);
                    parameter_t *oparams = ctx->block->parameters;
                    parameter_t *tparams = func.blocks[statement.conditional.true_block].parameters;
                    parameter_t *fparams = func.blocks[statement.conditional.false_block].parameters;
                    coy_function_builder_arg_imm_(&builder, arrlenu(tparams));
                    coy_cg_pshift(ctx, &builder, oparams, tparams);
                    coy_cg_pshift(ctx, &builder, oparams, fparams);
                    break;}
                case OP_NONE:
                    if (statement.conditional.condition->rhs.type != none) {
                        COY_ASSERT(0 && "Binary with no op!");
                    }
                    break;
                default:
                    COY_TODO("More conditions!");
                }
                break;
            default:
                errorf(ctx, "TODO: codegen stmt type %d", statement.type);
            }
        }
    }
    struct coy_function_ *fun = arraddnptr(ctx->functions, 1);
    coy_function_builder_finish_(&builder, fun);
    coy_module_inject_function_(ctx->module, func.base.name, fun);
}

coyc_cctx_t coyc_codegen(ast_root_t *root) {
    coyc_cctx_t ctx;
    ctx.err_msg = NULL;
    ctx.functions = NULL;
    ctx.block = NULL;
    coy_env_init(&ctx.env);
    ctx.module = coy_module_create_(&ctx.env, root->module_name, false);
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
            errorf(&ctx, "TODO codegen non-function entries");
        }
    }
    coy_module_link_(ctx.module);
    return ctx;
}

void coyc_cg_free(coyc_cctx_t ctx) {
    free(ctx.module);
}
