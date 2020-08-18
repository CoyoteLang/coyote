#include "compiler/lexer.h"
#include "compiler/ast.h"
#include "compiler/semalysis.h"
#include "compiler/codegen.h"
#include "function_builder.h"

#include "bytecode.h"

#include "stb_ds.h"

#include "test.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// TEMPORARY
#include "vm/compat_shims.h"

static const char src_lexer_parser[] =
        "module test;\n"
        // Need to decide how these are handled internally first.
        // "import std.io;\n"
        "int foo()\n"
        "{\n"
        "    return (((2 * 3)) / 1) - ((24 / 2) / 2);\n"
        "}\n"
;
static size_t count_lines(const char* str, size_t slen)
{
    size_t num = 0;
    while(slen--)
        if(str[slen] == '\n')
            ++num;
    return num;
}
// returned as `int` so that it can be passed directly into printf
static int num_digits(uint32_t n)
{
    if(!n) return 1;
    int nd = 0;
    do
    {
        ++nd;
        n /= 10;
    }
    while(n);
    return nd;
}

TEST(lexer)
{
    coyc_lexer_t lexer;
    PRECONDITION(coyc_lexer_init(&lexer, "<src_lexer_parser>", src_lexer_parser, sizeof(src_lexer_parser) - 1));

    size_t nlines = count_lines(src_lexer_parser, sizeof(src_lexer_parser) - 1);
    int nlines_ndigits = num_digits(nlines);

    uint32_t pline = (uint32_t)-1;
    for(;;)
    {
        coyc_token_t token = coyc_lexer_next(&lexer, COYC_LEXER_CATEGORY_PARSER);
        if(token.kind == COYC_TK_EOF)
            break;
        if(pline != token.range.head.line)  // if we had a newline
        {
            if(pline != (uint32_t)-1)
                printf("\n");
            pline = token.range.head.line;
            printf(TEST_COLOR(90) "%*" PRIu32 ":" TEST_COLOR(0) " %*s", nlines_ndigits, token.range.head.line, (int)token.range.head.col, "");
        }
        else if(pline != (uint32_t)-1)
            putchar(' ');
        coyc_token_dump_simple_DBG(&token, TEST_USE_COLOR);
        fflush(stdout);
        ASSERT_NE_INT(token.kind, COYC_TK_ERROR);
        if(token.kind == COYC_TK_ERROR)
            break;
    }
    if(pline != (uint32_t)-1)
        putchar('\n');

    coyc_lexer_deinit(&lexer);
}

TEST(parser)
{
    coyc_lexer_t lexer;
    PRECONDITION(coyc_lexer_init(&lexer, "<src_lexer_parser>", src_lexer_parser, sizeof(src_lexer_parser) - 1));

    coyc_pctx_t ctx;
    ast_root_t root;
    ctx.lexer = &lexer;
    ctx.root = &root;
    coyc_parse(&ctx);
    ASSERT_EQ_STR(ctx.err_msg, NULL);

    // Validate the results
    ASSERT(root.module_name);
    ASSERT_EQ_STR(root.module_name, "test");
    ASSERT(root.decls);
    ASSERT_EQ_UINT(arrlen(root.decls), 1);
    decl_t decl = root.decls[0];
    ASSERT(decl.base.type == function);
    function_t func = decl.function;
    ASSERT(func.base.name);
    ASSERT_EQ_STR(func.base.name, "foo");
    // Function types aren't resolved until sema runs
    ASSERT_EQ_INT(func.type.category, COY_TYPEINFO_CAT_INTERNAL_);
    ASSERT_EQ_INT(func.return_type.category, COY_TYPEINFO_CAT_INTEGER_);
    ASSERT_EQ_INT(func.return_type.u.integer.is_signed, true);
    ASSERT_EQ_INT(func.return_type.u.integer.width, 32);
    ASSERT(func.statements);
    ASSERT_EQ_UINT(arrlen(func.statements), 1);
    statement_t stmt = func.statements[0];
    ASSERT_EQ_INT(stmt.type, return_);
    ASSERT(stmt.return_.value);
    ASSERT_EQ_INT(stmt.return_.value->lhs.type, literal);
    ASSERT_EQ_INT(stmt.return_.value->rhs.type, none);
    // Integer type isn't resolved until sema runs
    ASSERT_EQ_INT(stmt.return_.value->type.category, COY_TYPEINFO_CAT_INTERNAL_);
    ASSERT_EQ_INT(stmt.return_.value->lhs.literal.value.integer.value, 0);
    // Finally, clean up. Note that I only do this so Valgrind doesn't complain;
    // this will be handled by the OS anyways.
    coyc_tree_free(&ctx);
    coyc_lexer_deinit(&lexer);
}

TEST(codegen)
{ 
    const char src[] =
        "module main;"
        "int foo(int a, int b, int c, int d)\n"
        "{\n"
        // 2, 3, 1, 24
        "    return (((a * b)) / c) - ((d / a) / a);\n"
        "}\n";
        // 9 - (8 / 3)
    coyc_pctx_t pctx;
    ast_root_t root;
    coyc_lexer_t lexer;
    PRECONDITION(coyc_lexer_init(&lexer, "<src>", src, sizeof(src) - 1));
    pctx.lexer = &lexer;
    pctx.root = &root;
    coyc_parse(&pctx);
    ASSERT_EQ_STR(pctx.err_msg, NULL);
    char *smsg = coyc_semalysis(&root);
    ASSERT_EQ_STR(smsg, NULL);

    coyc_cctx_t cctx = coyc_codegen(&root);
    ASSERT_EQ_STR(cctx.err_msg, NULL);
    ASSERT(cctx.module);
    ASSERT(cctx.module->functions);
    ASSERT_EQ_INT(arrlenu(cctx.module->functions), 1);
    ASSERT(coy_function_verify_(cctx.module->functions));

    coyc_tree_free(&pctx);
    coyc_lexer_deinit(&lexer);

    coy_env_t env;
    coy_env_init(&env);

    coy_context_t* ctx = coy_context_create(&env);
    coy_context_push_frame_(ctx, &cctx.module->functions[0], false);
    coy_push_uint(ctx, 2);
    coy_push_uint(ctx, 3);
    coy_push_uint(ctx, 1);
    coy_push_uint(ctx, 24);
    // will be replaced to use public API at some point, so just declare the function here
    extern void coy_vm_exec_frame_(coy_context_t*);
    coy_vm_exec_frame_(ctx);

    ASSERT_EQ_UINT(coy_slots_getval_(&ctx->top->slots, 0).u32, 0);

    coyc_cg_free(cctx);

    coy_env_deinit(&env);
}

const char *sema_test_srcs[] = {
    "module 1;",
    "module test;\n"
    "module 2;",
    "int a() {}",
    "module factorial;\n"
    "u32 factorial(uint num) {\n"
    "\tif (num < 2) return 1;\n"
    "\treturn factorial(num - 1) + factorial(num - 2);\n"
    "}\n"
};

const char *bad_parse_msgs[] = {
    "Expected identifier for module name!",
    "Duplicate module statement found!",
    "Missing a module statement!",
    NULL,
};

const char *bad_sema_msgs[] = {
    NULL,
    NULL,
    NULL,
    NULL,
};

TEST(semantic_analysis) {
    size_t src_size = sizeof(sema_test_srcs) / sizeof(*sema_test_srcs);
    size_t pmsg_size = sizeof(bad_parse_msgs) / sizeof(*bad_parse_msgs);
    size_t smsg_size = sizeof(bad_sema_msgs) / sizeof(*bad_sema_msgs);
    PRECONDITION(src_size == pmsg_size && "Forgot to add a parser error message (or NULL)");
    PRECONDITION(src_size == smsg_size && "Forgot to add a compilation error message (or NULL)");
    for (size_t i = 0; i < src_size; i += 1) {
        PRECONDITION(sema_test_srcs[i]);
        coyc_lexer_t lexer;
        PRECONDITION(coyc_lexer_init(&lexer, "<semalysis_test>", sema_test_srcs[i], strlen(sema_test_srcs[i])));
        ast_root_t root;
        coyc_pctx_t pctx;
        pctx.lexer = &lexer;
        pctx.root = &root;
        coyc_parse(&pctx);
        ASSERT_EQ_STR(pctx.err_msg, bad_parse_msgs[i]);
        if (!pctx.err_msg) {
            // Parser is good, let's check semalysis
            ASSERT_EQ_STR(coyc_semalysis(&root), bad_sema_msgs[i]);
            if (!bad_sema_msgs[i]) {
                // Semalysis modifies the tree *in place*, so we can just check it now
                switch (i) {
                    case 3:{
                        ASSERT_EQ_INT(arrlenu(root.decls), 1);
                        ASSERT_EQ_INT(root.decls[0].base.type, function);
                        ASSERT_EQ_STR(root.decls[0].base.name, "factorial");
                        ASSERT_EQ_INT(root.decls[0].function.return_type.category, COY_TYPEINFO_CAT_INTEGER_);
                        ASSERT_EQ_INT(root.decls[0].function.return_type.u.integer.is_signed, false);
                        ASSERT_EQ_INT(root.decls[0].function.return_type.u.integer.width, 32);
                        ASSERT_EQ_INT(root.decls[0].function.type.category, COY_TYPEINFO_CAT_FUNCTION_);
                        ASSERT(coy_type_eql_(*root.decls[0].function.type.u.function.rtype, root.decls[0].function.return_type));
                        ASSERT_EQ_INT(arrlen(root.decls[0].function.parameters), 1);
                        ASSERT(root.decls[0].function.type.u.function.ptypes);
                        ASSERT(root.decls[0].function.type.u.function.ptypes[0]);
                        size_t count = 0;
                        for (struct coy_typeinfo_ **T = root.decls[0].function.type.u.function.ptypes; *T; T += 1, count = count + 1) {
                            ASSERT_EQ_PTR(*T, root.decls[0].function.parameters + count);
                        }
                        ASSERT_EQ_INT(count, 1);
                        break;}
                    default: ASSERT_TODO("semalysis test%lu", i);
                }
            }
            coyc_tree_free(&pctx);
        }
        else {
            PRECONDITION(!bad_sema_msgs[i]);
        }
        coyc_lexer_deinit(&lexer);
    }
}

TEST(vm_basic)
{
    static struct coy_typeinfo_ ti_int = {
        COY_TYPEINFO_CAT_INTEGER_,
        {.integer={
            .is_signed=true,
            .width=32,
        }},
        NULL, NULL,
    };
    static struct coy_typeinfo_* ti_params_int_int[] = {
        &ti_int,
        &ti_int,
        NULL,
    };
    static struct coy_typeinfo_ ti_function_int_int_int = {
        COY_TYPEINFO_CAT_FUNCTION_,
        {.function={
            .rtype = &ti_int,
            .ptypes = ti_params_int_int,
        }},
        NULL, NULL,
    };

    static const struct coy_function_block_ in_blocks[] = {
        {0,2,NULL},  //entry
    };
    static const union coy_instruction_ in_instrs[] = {
        //.0_entry(2)   @0
        /*-*/   {.op={COY_OPCODE_ADD, COY_OPFLG_TYPE_UINT32, 0, 2}},
        /*-*/   {.arg={0,0}},
        /*-*/   {.arg={1,0}},
        /*-*/   {.op={COY_OPCODE_RET, 0, 0, 1}},
        /*-*/   {.arg={2,0}},
    };

    struct coy_function_ func;
    coy_function_init_empty_(&func, &ti_function_int_int_int, 0);
    stbds_arrsetlen(func.u.coy.blocks, sizeof(in_blocks) / sizeof(*in_blocks));
    memcpy(func.u.coy.blocks, in_blocks, sizeof(in_blocks));
    stbds_arrsetlen(func.u.coy.instrs, sizeof(in_instrs) / sizeof(*in_instrs));
    memcpy(func.u.coy.instrs, in_instrs, sizeof(in_instrs));
    coy_function_coy_compute_maxslots_(&func);

    coy_env_t env;
    coy_env_init(&env);

    struct coy_module_* module = coy_module_create_(&env, "main", false);
    coy_module_inject_function_(module, "add", &func);

    coy_context_t* ctx = coy_context_create(&env);

    coy_ensure_slots(ctx, 2);
    coy_set_uint(ctx, 0, 5);
    coy_set_uint(ctx, 1, 7);
    ASSERT(coy_call(ctx, "main", "add"));
    ASSERT_EQ_INT(coy_get_uint(ctx, 0), 5 + 7);

    printf("RESULT: %" PRIu32 "\n", coy_get_uint(ctx, 0));

    coy_env_deinit(&env);
}

static void function_builder_prepare(struct coy_function_* func, bool precondition)
{
    static struct coy_typeinfo_ ti_int = {
        COY_TYPEINFO_CAT_INTEGER_,
        {.integer={
            .is_signed=true,
            .width=32,
        }},
        NULL, NULL,
    };
    static struct coy_typeinfo_* ti_params_int[] = {
        &ti_int,
        NULL,
    };
    static struct coy_typeinfo_ ti_function_int_int = {
        COY_TYPEINFO_CAT_FUNCTION_,
        {.function={
            .rtype = &ti_int,
            .ptypes = ti_params_int,
        }},
        NULL, NULL,
    };

    struct coy_function_builder_ builder;
    PRECONDITION(coy_function_builder_init_(&builder, &ti_function_int_int, 0));

/*
u32 factorial(u32 num)
.0_entry(num):
    jmp .1_test($0, 1)
.1_test(num,acc):
    jmpc lt 0, $0,
        .2_loop($0,$1),
        .3_end($1)
.2_loop(num,acc):
    $2 = sub $0, 1
    $5 = mul $0, $1
    jmp .1_test($2, $5)
.3_end(acc):
    ret $0
*/
    uint32_t b0_entry = coy_function_builder_block_(&builder, 1, NULL, 0);
    uint32_t b1_test = coy_function_builder_block_(&builder, 2, NULL, 0);
    uint32_t b2_loop = coy_function_builder_block_(&builder, 2, NULL, 0);
    uint32_t b3_end = coy_function_builder_block_(&builder, 1, NULL, 0);

    coy_function_builder_useblock_(&builder, b0_entry);
    {
        coy_function_builder_op_(&builder, COY_OPCODE_JMP, 0, false);
            coy_function_builder_arg_imm_(&builder, b1_test);
            coy_function_builder_arg_reg_(&builder, 0);
            coy_function_builder_arg_const_val_(&builder, (union coy_register_){.u32=1});
    }
    coy_function_builder_useblock_(&builder, b1_test);
    {
        coy_function_builder_op_(&builder, COY_OPCODE_JMPC, COY_OPFLG_CMP_LT|COY_OPFLG_TYPE_UINT32, false);
            coy_function_builder_arg_const_val_(&builder, (union coy_register_){.u32=0});
            coy_function_builder_arg_reg_(&builder, 0);
            coy_function_builder_arg_imm_(&builder, b2_loop);
            coy_function_builder_arg_imm_(&builder, b3_end);
            coy_function_builder_arg_imm_(&builder, 2);
            coy_function_builder_arg_reg_(&builder, 0);
            coy_function_builder_arg_reg_(&builder, 1);
            coy_function_builder_arg_reg_(&builder, 1);
    }
    coy_function_builder_useblock_(&builder, b2_loop);
    {
        uint32_t sub = coy_function_builder_op_(&builder, COY_OPCODE_SUB, COY_OPFLG_TYPE_UINT32, false);
            coy_function_builder_arg_reg_(&builder, 0);
            coy_function_builder_arg_const_val_(&builder, (union coy_register_){.u32=1});
        uint32_t mul = coy_function_builder_op_(&builder, COY_OPCODE_MUL, COY_OPFLG_TYPE_UINT32, false);
            coy_function_builder_arg_reg_(&builder, 0);
            coy_function_builder_arg_reg_(&builder, 1);
        coy_function_builder_op_(&builder, COY_OPCODE_JMP, 0, false);
            coy_function_builder_arg_imm_(&builder, b1_test);
            coy_function_builder_arg_reg_(&builder, sub);
            coy_function_builder_arg_reg_(&builder, mul);
    }
    coy_function_builder_useblock_(&builder, b3_end);
    {
        coy_function_builder_op_(&builder, COY_OPCODE_RET, 0, false);
            coy_function_builder_arg_reg_(&builder, 0);
    }

    coy_function_builder_finish_(&builder, func);
    if(precondition)
        PRECONDITION(coy_function_verify_(func));
    else
        ASSERT(coy_function_verify_(func));
}
TEST(function_builder_verify)
{
    struct coy_function_ func;
    function_builder_prepare(&func, false);
}
TEST(vm_factorial)
{
    struct coy_function_ func;
    function_builder_prepare(&func, true);

    coy_env_t env;
    coy_env_init(&env);

    struct coy_module_* module = coy_module_create_(&env, "main", false);
    coy_module_inject_function_(module, "factorial", &func);

    coy_context_t* ctx = coy_context_create(&env);

    coy_ensure_slots(ctx, 1);
    coy_set_uint(ctx, 0, 6);
    ASSERT(coy_call(ctx, "main", "factorial"));
    ASSERT_EQ_INT(coy_get_uint(ctx, 0), 720);

    printf("RESULT: %" PRIu32 "\n", coy_get_uint(ctx, 0));

    coy_env_deinit(&env);
}

TEST(vm_factorial_call)
{
    ASSERT_TODO("implement coy_function_verify_{call,retcall}_, and the calls themselves in vm.c");
    static struct coy_typeinfo_ ti_int = {
        COY_TYPEINFO_CAT_INTEGER_,
        {.integer={
            .is_signed=true,
            .width=32,
        }},
        NULL, NULL,
    };
    static struct coy_typeinfo_* ti_params_int_int[] = {
        &ti_int,
        &ti_int,
        NULL,
    };
    static struct coy_typeinfo_ ti_function_int_int_int = {
        COY_TYPEINFO_CAT_FUNCTION_,
        {.function={
            .rtype = &ti_int,
            .ptypes = ti_params_int_int,
        }},
        NULL, NULL,
    };
    static struct coy_typeinfo_ ti_function_int_int = {
        COY_TYPEINFO_CAT_FUNCTION_,
        {.function={
            .rtype = &ti_int,
            .ptypes = ti_params_int_int + 1, //< the offset skips the first `int`
        }},
        NULL, NULL,
    };


    coy_env_t env;
    coy_env_init(&env);

    struct coy_module_* module = coy_module_create_(&env, "main", false);

    struct coy_function_builder_ builder;
    /* ========== factorial ========== */
/*
u32 factorial(u32 num)
.0_entry(num):
    $1 = call factpart($0, 1)
    ret $1
*/
    PRECONDITION(coy_function_builder_init_(&builder, &ti_function_int_int, 0));
    struct coy_function_ func_factorial;
    {
        uint32_t b0_entry = coy_function_builder_block_(&builder, 1, NULL, 0);

        coy_function_builder_useblock_(&builder, b0_entry);
        {
            // this one could be a tail call, but we want to test normal calls, too
            uint32_t callret = coy_function_builder_op_(&builder, COY_OPCODE_CALL, 0, false);
                coy_function_builder_arg_const_sym_(&builder, "main;factpart");
                coy_function_builder_arg_reg_(&builder, 0);
                coy_function_builder_arg_const_val_(&builder, (union coy_register_){.u32=1});
            coy_function_builder_op_(&builder, COY_OPCODE_RET, 0, false);
                coy_function_builder_arg_reg_(&builder, callret);
        }

        coy_function_builder_finish_(&builder, &func_factorial);
        PRECONDITION(coy_function_verify_(&func_factorial));
        coy_module_inject_function_(module, "factorial", &func_factorial);
    }

    /* ========== factpart ========== */
/*
u32 factpart(u32 num, u32 acc)
.0_entry(num,acc):
    jmpc lt 0, $1,
        .1_call($0,$1),
        .2_end($1)
.1_call(num,acc):
    $2 = sub $0, 1
    $5 = mul $0, $1
    retcall factpart($2, $5)
.2_end(acc):
    ret $0
*/
    PRECONDITION(coy_function_builder_init_(&builder, &ti_function_int_int_int, 0));
    struct coy_function_ func_factpart;
    {
        uint32_t b0_entry = coy_function_builder_block_(&builder, 2, NULL, 0);
        uint32_t b1_call = coy_function_builder_block_(&builder, 2, NULL, 0);
        uint32_t b2_end = coy_function_builder_block_(&builder, 1, NULL, 0);

        coy_function_builder_useblock_(&builder, b0_entry);
        {
            coy_function_builder_op_(&builder, COY_OPCODE_JMPC, COY_OPFLG_CMP_LT|COY_OPFLG_TYPE_UINT32, false);
                coy_function_builder_arg_const_val_(&builder, (union coy_register_){.u32=0});
                coy_function_builder_arg_reg_(&builder, 0);
                coy_function_builder_arg_imm_(&builder, b1_call);
                coy_function_builder_arg_imm_(&builder, b2_end);
                coy_function_builder_arg_imm_(&builder, 2);
                coy_function_builder_arg_reg_(&builder, 0);
                coy_function_builder_arg_reg_(&builder, 1);
                coy_function_builder_arg_reg_(&builder, 1);
        }
        coy_function_builder_useblock_(&builder, b1_call);
        {
            uint32_t sub = coy_function_builder_op_(&builder, COY_OPCODE_SUB, COY_OPFLG_TYPE_UINT32, false);
                coy_function_builder_arg_reg_(&builder, 0);
                coy_function_builder_arg_const_val_(&builder, (union coy_register_){.u32=1});
            uint32_t mul = coy_function_builder_op_(&builder, COY_OPCODE_MUL, COY_OPFLG_TYPE_UINT32, false);
                coy_function_builder_arg_reg_(&builder, 0);
                coy_function_builder_arg_reg_(&builder, 1);
            coy_function_builder_op_(&builder, COY_OPCODE_RETCALL, 0, false);
                coy_function_builder_arg_const_sym_(&builder, "main;factpart");
                coy_function_builder_arg_reg_(&builder, sub);
                coy_function_builder_arg_reg_(&builder, mul);
        }
        coy_function_builder_useblock_(&builder, b2_end);
        {
            coy_function_builder_op_(&builder, COY_OPCODE_RET, 0, false);
                coy_function_builder_arg_reg_(&builder, 0);
        }

        coy_function_builder_finish_(&builder, &func_factpart);
        PRECONDITION(coy_function_verify_(&func_factpart));
        coy_module_inject_function_(module, "factpart", &func_factpart);
    }

    /* ========== execute ========== */
    coy_context_t* ctx = coy_context_create(&env);

    coy_ensure_slots(ctx, 1);
    coy_set_uint(ctx, 0, 6);
    ASSERT(coy_call(ctx, "main", "factorial"));
    ASSERT_EQ_INT(coy_get_uint(ctx, 0), 720);

    printf("RESULT: %" PRIu32 "\n", coy_get_uint(ctx, 0));

    coy_env_deinit(&env);
}

int main()
{
    TEST_EXEC(lexer);
    TEST_EXEC(parser);
    TEST_EXEC(semantic_analysis);
    TEST_EXEC(vm_basic);
    TEST_EXEC(codegen);
    TEST_EXEC(function_builder_verify);
    TEST_EXEC(vm_factorial);
    TEST_EXEC(vm_factorial_call);
    return TEST_REPORT();
}
