#include "compiler/lexer.h"
#include "compiler/ast.h"
#include "compiler/semalysis.h"
#include "compiler/codegen.h"
#include "function_builder.h"

#include "vm/env.h"
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

// This test is for catching stb_ds bugs (currently, to ensure our fix for #775 doesn't regress).
TEST(stb_ds)
{
    struct test_entry_
    {
        char* key;
        const char* value;
    };
    struct test_entry_* sh = NULL;

    stbds_shput(sh, "main", "this is `main`");  //< sh["main"] = "this is `main`"

    struct test_entry_* entry = stbds_shgetp_null(sh, "main");   //< entry = sh["main"];

    ASSERT(entry);
    // and indeed, the test itself is a fail
    ASSERT_EQ_STR(entry->key, "main");
    ASSERT_EQ_STR(entry->value, "this is `main`");
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
    ASSERT(func.blocks);
    ASSERT_EQ_UINT(arrlenu(func.blocks), 1);
    block_t block = func.blocks[0];
    ASSERT_EQ_UINT(arrlen(block.statements), 1);
    statement_t stmt = block.statements[0];
    ASSERT_EQ_INT(stmt.type, return_);
    ASSERT(stmt.expr.value);
    ASSERT_EQ_INT(stmt.expr.value->lhs.type, literal);
    ASSERT_EQ_INT(stmt.expr.value->rhs.type, none);
    // Integer type isn't resolved until sema runs
    ASSERT_EQ_INT(stmt.expr.value->type.category, COY_TYPEINFO_CAT_INTERNAL_);
    ASSERT_EQ_INT(stmt.expr.value->lhs.literal.value.integer.value, 0);
    // Finally, clean up. Note that I only do this so Valgrind doesn't complain;
    // this will be handled by the OS anyways.
    coyc_tree_free(&ctx);
    coyc_lexer_deinit(&lexer);
}

const char *sema_test_srcs[] = {
    "module 1;",

    "module test;\n"
    "module 2;",

    "int a() {}",

    "module function_call;\n"
    "\n"
    "int a() {}\n"
    "int b() {\n"
    "\ta();\n"
    "}\n",

    "module main;"
    "int foo(int a, int b, int c, int d)\n"
    "{\n"
    // 2, 3, 1, 24
    "    return (((a * b)) / c) - ((d / a) / a);\n"
    "}\n",

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
    NULL,
    NULL,
};

const char *bad_sema_msgs[] = {
    NULL,
    NULL,
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
        if (i == 4) {
            // Covered by codegen, I'll add this eventually
            printf("Reminder to add in sema4 test, not important though.\n");
            continue;
        }
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
                    case 3:
                        break;
                    case 5:{
                        ASSERT_EQ_INT(arrlenu(root.decls), 1);
                        ASSERT_EQ_INT(root.decls[0].base.type, function);
                        ASSERT_EQ_STR(root.decls[0].base.name, "factorial");
                        function_t func = root.decls[0].function;
                        ASSERT_EQ_INT(func.return_type.category, COY_TYPEINFO_CAT_INTEGER_);
                        ASSERT_EQ_INT(func.return_type.u.integer.is_signed, false);
                        ASSERT_EQ_INT(func.return_type.u.integer.width, 32);
                        ASSERT_EQ_INT(func.type.category, COY_TYPEINFO_CAT_FUNCTION_);
                        ASSERT(coy_type_eql_(*func.type.u.function.rtype, func.return_type));
                        ASSERT_EQ_INT(arrlen(func.parameters), 1);
                        ASSERT(func.type.u.function.ptypes);
                        ASSERT(func.type.u.function.ptypes[0]);
                        const struct coy_typeinfo_ *ptype = func.type.u.function.ptypes[0];
                        ASSERT_EQ_INT(ptype->category, COY_TYPEINFO_CAT_INTEGER_);
                        ASSERT_EQ_INT(ptype->u.integer.is_signed, false);
                        ASSERT_EQ_INT(ptype->u.integer.width, 32);
                        size_t count = 0;
                        for (const struct coy_typeinfo_ * const*T = root.decls[0].function.type.u.function.ptypes; *T; T += 1, count = count + 1) {
                            ASSERT_EQ_PTR(*T, root.decls[0].function.parameters + count);
                            ASSERT_EQ_PTR(*T, func.parameters + count);
                        }
                        ASSERT_EQ_INT(count, 1);
                        // 3 blocks: conditional jump, `return 1`, `return factorial`
                        ASSERT_EQ_INT(arrlenu(func.blocks), 3);
                        // Conditional
                        ASSERT_EQ_INT(arrlenu(func.blocks[0].statements), 1);
                        statement_t cond= func.blocks[0].statements[0];
                        ASSERT_EQ_INT(cond.type, conditional);
                        // Return 1
                        ASSERT_EQ_INT(arrlenu(func.blocks[1].statements), 1);
                        // Tail call
                        ASSERT_EQ_INT(arrlenu(func.blocks[2].statements), 1);
                        break;}
                    default: ASSERT_TODO("semalysis test%" PRIu64, (uint64_t)i);
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

TEST(typeinfo_integer)
{
    // check whether all the integers are correct
    coy_env_t env;
    PRECONDITION(coy_env_init(&env));

    ASSERT_EQ_STR(coy_typeinfo_integer_(&env, 8, true)->repr, "byte");
    ASSERT_EQ_STR(coy_typeinfo_integer_(&env, 8, false)->repr, "ubyte");
    ASSERT_EQ_STR(coy_typeinfo_integer_(&env, 16, true)->repr, "short");
    ASSERT_EQ_STR(coy_typeinfo_integer_(&env, 16, false)->repr, "ushort");
    ASSERT_EQ_STR(coy_typeinfo_integer_(&env, 32, true)->repr, "int");
    ASSERT_EQ_STR(coy_typeinfo_integer_(&env, 32, false)->repr, "uint");
    ASSERT_EQ_STR(coy_typeinfo_integer_(&env, 64, true)->repr, "long");
    ASSERT_EQ_STR(coy_typeinfo_integer_(&env, 64, false)->repr, "ulong");
    //ASSERT_EQ_STR(coy_typeinfo_integer_(&env, 128, true)->repr, "cent");
    //ASSERT_EQ_STR(coy_typeinfo_integer_(&env, 128, false)->repr, "ucent");

    coy_env_deinit(&env);
}

TEST(typeinfo_tensor)
{
    coy_env_t env;
    PRECONDITION(coy_env_init(&env));

    struct coy_typeinfo_* ti_int = coy_typeinfo_integer_(&env, 32, true);
    struct coy_typeinfo_* ti_int_tensor = coy_typeinfo_tensor_(&env, ti_int, (const uint32_t[]){3}, 1);

    ASSERT_EQ_STR(ti_int_tensor->repr, "int#3");
    ASSERT_EQ_STR(coy_typeinfo_tensor_(&env, ti_int, (const uint32_t[]){3,2}, 2)->repr, "int#[3,2]");
    ASSERT_EQ_STR(coy_typeinfo_tensor_(&env, ti_int, (const uint32_t[]){5,5,2}, 3)->repr, "int#[5,5,2]");
    ASSERT_EQ_STR(coy_typeinfo_tensor_(&env, ti_int_tensor, (const uint32_t[]){1,2,3,4}, 4)->repr, "int#[3,1,2,3,4]");

    coy_env_deinit(&env);
}

TEST(typeinfo_array)
{
    coy_env_t env;
    PRECONDITION(coy_env_init(&env));

    struct coy_typeinfo_* ti_int = coy_typeinfo_integer_(&env, 32, true);
    struct coy_typeinfo_* ti_int_array = coy_typeinfo_array_(&env, ti_int, 1);

    ASSERT_EQ_STR(ti_int_array->repr, "int[]");
    ASSERT_EQ_STR(coy_typeinfo_array_(&env, ti_int, 2)->repr, "int[,]");
    ASSERT_EQ_STR(coy_typeinfo_array_(&env, ti_int, 3)->repr, "int[,,]");
    ASSERT_EQ_STR(coy_typeinfo_array_(&env, ti_int_array, 4)->repr, "int[][,,,]");

    coy_env_deinit(&env);
}

TEST(typeinfo_function)
{
    coy_env_t env;
    PRECONDITION(coy_env_init(&env));

    struct coy_typeinfo_* ti_int = coy_typeinfo_integer_(&env, 32, true);
    struct coy_typeinfo_* ti_function_int_int_int = coy_typeinfo_function_(&env, ti_int, (const struct coy_typeinfo_*[]){ti_int, ti_int}, 2);

    ASSERT_EQ_STR(ti_function_int_int_int->repr, "int(int,int)");

    coy_env_deinit(&env);
}

TEST(typeinfo_intern_dedup)
{
    coy_env_t env;
    PRECONDITION(coy_env_init(&env));

    struct coy_typeinfo_* ti_int = coy_typeinfo_integer_(&env, 32, true);
    struct coy_typeinfo_* ti_func1 = coy_typeinfo_function_(&env, ti_int, (const struct coy_typeinfo_*[]){ti_int, ti_int}, 2);
    struct coy_typeinfo_* ti_func2 = coy_typeinfo_function_(&env, ti_int, (const struct coy_typeinfo_*[]){ti_int, ti_int}, 2);

    ASSERT_EQ_PTR(ti_func1, ti_func2);
    coy_env_deinit(&env);
}

TEST(codegen)
{
        // 9 - (8 / 3)
    coyc_pctx_t pctx;
    ast_root_t root;
    coyc_lexer_t lexer;
    for (int i = 4; i <= 5; i += 1) {
        const char *src = sema_test_srcs[i];
        PRECONDITION(coyc_lexer_init(&lexer, "<src>", src, strlen(src) - 1));
        pctx.lexer = &lexer;
        pctx.root = &root;
        coyc_parse(&pctx);
        ASSERT_EQ_STR(pctx.err_msg, NULL);
        char *smsg = coyc_semalysis(&root);
        ASSERT_EQ_STR(smsg, NULL);

        coyc_cctx_t cctx = coyc_codegen(&root);
        ASSERT_EQ_STR(cctx.err_msg, NULL);
        ASSERT(cctx.module);

        coyc_tree_free(&pctx);
        coyc_lexer_deinit(&lexer);

        coy_context_t* ctx = coy_context_create(&cctx.env);
        switch (i) {
        case 4:
            coy_ensure_slots(ctx, 4);
            coy_set_uint(ctx, 0, 2);
            coy_set_uint(ctx, 1, 3);
            coy_set_uint(ctx, 2, 1);
            coy_set_uint(ctx, 3, 24);
            ASSERT(coy_call(ctx, "main", "foo"));
            ASSERT_EQ_INT(coy_get_uint(ctx, 0), 0);
            break;
        default:
            ASSERT_TODO("codegen");
        }
        coyc_cg_free(cctx);
    }
}

TEST(vm_basic)
{
    coy_env_t env;
    PRECONDITION(coy_env_init(&env));

    struct coy_typeinfo_* ti_int = coy_typeinfo_integer_(&env, 32, true);
    struct coy_typeinfo_* ti_function_int_int_int = coy_typeinfo_function_(&env, ti_int, (const struct coy_typeinfo_*[]){ti_int, ti_int}, 2);

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
    coy_function_init_empty_(&func, ti_function_int_int_int, 0);
    stbds_arrsetlen(func.u.coy.blocks, sizeof(in_blocks) / sizeof(*in_blocks));
    memcpy(func.u.coy.blocks, in_blocks, sizeof(in_blocks));
    stbds_arrsetlen(func.u.coy.instrs, sizeof(in_instrs) / sizeof(*in_instrs));
    memcpy(func.u.coy.instrs, in_instrs, sizeof(in_instrs));
    coy_function_coy_compute_maxslots_(&func);

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
    coy_env_t env;
    PRECONDITION(coy_env_init(&env));

    struct coy_typeinfo_* ti_int = coy_typeinfo_integer_(&env, 32, true);
    struct coy_typeinfo_* ti_function_int_int = coy_typeinfo_function_(&env, ti_int, (const struct coy_typeinfo_*[]){ti_int}, 1);

    struct coy_function_builder_ builder;
    PRECONDITION(coy_function_builder_init_(&builder, ti_function_int_int, 0));

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

    coy_env_deinit(&env);
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
    coy_env_t env;
    coy_env_init(&env);

    struct coy_typeinfo_* ti_int = coy_typeinfo_integer_(&env, 32, true);
    struct coy_typeinfo_* ti_function_int_int = coy_typeinfo_function_(&env, ti_int, (const struct coy_typeinfo_*[]){ti_int}, 1);
    struct coy_typeinfo_* ti_function_int_int_int = coy_typeinfo_function_(&env, ti_int, (const struct coy_typeinfo_*[]){ti_int,ti_int}, 2);

    struct coy_module_* module = coy_module_create_(&env, "main", false);

    struct coy_function_builder_ builder;
    /* ========== factorial ========== */
/*
u32 factorial(u32 num)
.0_entry(num):
    $1 = call factpart($0, 1)
    ret $1
*/
    PRECONDITION(coy_function_builder_init_(&builder, ti_function_int_int, 0));
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
    PRECONDITION(coy_function_builder_init_(&builder, ti_function_int_int_int, 0));
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
        coy_module_inject_function_(module, "factpart", &func_factpart);
    }
    ASSERT(coy_module_link_(module));
    PRECONDITION(coy_function_verify_(&func_factorial));
    PRECONDITION(coy_function_verify_(&func_factpart));

    /* ========== execute ========== */
    coy_context_t* ctx = coy_context_create(&env);

    coy_ensure_slots(ctx, 1);
    coy_set_uint(ctx, 0, 6);
    ASSERT(coy_call(ctx, "main", "factorial"));
    ASSERT_EQ_INT(coy_get_uint(ctx, 0), 720);

    printf("RESULT: %" PRIu32 "\n", coy_get_uint(ctx, 0));

    coy_env_deinit(&env);
}

static int32_t nat_main_add(coy_context_t* ctx, void* udata)
{
    // they're made 3 separate statements for debugging reasons (for gdb stepping)
    uint32_t a = coy_get_uint(ctx, 0);
    uint32_t b = coy_get_uint(ctx, 1);
    coy_set_uint(ctx, 0, a + b);
    return 1;
}
static void vm_native_call_prepare(coy_env_t* env, bool use_main, bool use_main_retcall)
{
    coy_env_init(env);

    struct coy_typeinfo_* ti_uint = coy_typeinfo_integer_(env, 32, false);
    struct coy_typeinfo_* ti_function_uint = coy_typeinfo_function_(env, ti_uint, NULL, 0);
    struct coy_typeinfo_* ti_function_uint_uint_uint = coy_typeinfo_function_(env, ti_uint, (const struct coy_typeinfo_*[]){ti_uint,ti_uint}, 2);

    struct coy_module_* module = coy_module_create_(env, "main", false);

    static struct coy_function_ f_main_add;
    ASSERT(coy_function_init_native_(&f_main_add, ti_function_uint_uint_uint, COY_FUNCTION_ATTRIB_NATIVE_, nat_main_add, NULL));
    coy_module_inject_function_(module, "add", &f_main_add);

    static struct coy_function_ f_main_main;
    if(use_main)
    {
        struct coy_function_builder_ builder;
        PRECONDITION(coy_function_builder_init_(&builder, ti_function_uint, 0));

        coy_function_builder_block_(&builder, 0, NULL, 0);
        {
            uint32_t r = coy_function_builder_op_(&builder, use_main_retcall ? COY_OPCODE_RETCALL : COY_OPCODE_CALL, 0, false);
                coy_function_builder_arg_const_sym_(&builder, "main;add");
                coy_function_builder_arg_const_val_(&builder, (union coy_register_){.u32=5});
                coy_function_builder_arg_const_val_(&builder, (union coy_register_){.u32=7});
                if(!use_main_retcall)
                {
                    coy_function_builder_op_(&builder, COY_OPCODE_RET, 0, false);
                    coy_function_builder_arg_reg_(&builder, r);
                }
        }
        coy_function_builder_finish_(&builder, &f_main_main);
        coy_module_inject_function_(module, "main", &f_main_main);
    }

    PRECONDITION(coy_module_link_(module));
    if(use_main)
        PRECONDITION(coy_function_verify_(&f_main_main));
}
TEST(vm_native_call)
{
    coy_env_t env;
    vm_native_call_prepare(&env, true, false);

    coy_context_t* ctx = coy_context_create(&env);

    // test if native->coyote->native calls work
    ASSERT(coy_call(ctx, "main", "main"));
    ASSERT_EQ_INT(coy_get_uint(ctx, 0), 5 + 7);
    printf("RESULT: %" PRIu32 "\n", coy_get_uint(ctx, 0));

    coy_env_deinit(&env);
}
TEST(vm_native_retcall)
{
    coy_env_t env;
    vm_native_call_prepare(&env, true, true);

    coy_context_t* ctx = coy_context_create(&env);

    // test if native->coyote->native calls work
    ASSERT(coy_call(ctx, "main", "main"));
    ASSERT_EQ_INT(coy_get_uint(ctx, 0), 5 + 7);
    printf("RESULT: %" PRIu32 "\n", coy_get_uint(ctx, 0));

    coy_env_deinit(&env);
}
TEST(vm_native_call_direct)
{
    coy_env_t env;
    vm_native_call_prepare(&env, false, false);

    coy_context_t* ctx = coy_context_create(&env);

    // test if native->native calls work
    coy_ensure_slots(ctx, 2);
    coy_set_uint(ctx, 0, 10);
    coy_set_uint(ctx, 1, 15);
    ASSERT(coy_call(ctx, "main", "add"));
    ASSERT_EQ_INT(coy_get_uint(ctx, 0), 10 + 15);
    printf("RESULT: %" PRIu32 "\n", coy_get_uint(ctx, 0));

    coy_env_deinit(&env);
}

TEST(vm_vector2_add)
{
    coy_env_t env;
    PRECONDITION(coy_env_init(&env));

    struct coy_typeinfo_* ti_int = coy_typeinfo_integer_(&env, 32, true);
    struct coy_typeinfo_* ti_int2 = coy_typeinfo_tensor_(&env, ti_int, (const uint32_t[]){2}, 1);
    struct coy_typeinfo_* ti_function_int2_int2_int2 = coy_typeinfo_function_(&env, ti_int2, (const struct coy_typeinfo_*[]){ti_int2, ti_int2}, 2);

    struct coy_typeinfo_* ti_uint = coy_typeinfo_integer_(&env, 32, false);
    struct coy_typeinfo_* ti_uint2 = coy_typeinfo_tensor_(&env, ti_uint, (const uint32_t[]){2}, 1);
    struct coy_typeinfo_* ti_function_uint2_uint2_uint2 = coy_typeinfo_function_(&env, ti_uint2, (const struct coy_typeinfo_*[]){ti_uint2, ti_uint2}, 2);

    struct coy_module_* module = coy_module_create_(&env, "main", false);

    struct coy_function_builder_ builder;

    PRECONDITION(coy_function_builder_init_(&builder, ti_function_int2_int2_int2, 0));
    struct coy_function_ func_add_int2;
    {
        coy_function_builder_block_(&builder, 2, NULL, 0);
        {
            // this one could be a tail call, but we want to test normal calls, too
            uint32_t add = coy_function_builder_op_(&builder, COY_OPCODE_ADD, COY_OPFLG_TYPE_INT32X2_TEMP, false);
                coy_function_builder_arg_reg_(&builder, 0);
                coy_function_builder_arg_reg_(&builder, 1);
            coy_function_builder_op_(&builder, COY_OPCODE_RET, 0, false);
                coy_function_builder_arg_reg_(&builder, add);
        }

        coy_function_builder_finish_(&builder, &func_add_int2);
        coy_module_inject_function_(module, "add_int2", &func_add_int2);
    }

    PRECONDITION(coy_function_builder_init_(&builder, ti_function_uint2_uint2_uint2, 0));
    struct coy_function_ func_add_uint2;
    {
        coy_function_builder_block_(&builder, 2, NULL, 0);
        {
            // this one could be a tail call, but we want to test normal calls, too
            uint32_t add = coy_function_builder_op_(&builder, COY_OPCODE_ADD, COY_OPFLG_TYPE_UINT32X2_TEMP, false);
                coy_function_builder_arg_reg_(&builder, 0);
                coy_function_builder_arg_reg_(&builder, 1);
            coy_function_builder_op_(&builder, COY_OPCODE_RET, 0, false);
                coy_function_builder_arg_reg_(&builder, add);
        }

        coy_function_builder_finish_(&builder, &func_add_uint2);
        coy_module_inject_function_(module, "add_uint2", &func_add_uint2);
    }
    PRECONDITION(coy_module_link_(module));

    ASSERT(coy_function_verify_(&func_add_int2));
    ASSERT(coy_function_verify_(&func_add_uint2));

    coy_context_t* ctx = coy_context_create(&env);

    size_t vsize;

    const int32_t* vint;
    coy_ensure_slots(ctx, 2);
    coy_set_uint_vector(ctx, 0, (const uint32_t[]){5, 7}, 2);
    coy_set_uint_vector(ctx, 1, (const uint32_t[]){10, 12}, 2);
    ASSERT(coy_call(ctx, "main", "add_int2"));
    vint = (const int32_t*)coy_get_uint_vector(ctx, 0, &vsize);
    ASSERT(vint);
    ASSERT_EQ_INT(vsize, 2);
    ASSERT_EQ_INT(vint[0], 15);
    ASSERT_EQ_INT(vint[1], 19);

    const uint32_t* vuint;
    coy_ensure_slots(ctx, 2);
    coy_set_uint_vector(ctx, 0, (const uint32_t[]){5, 7}, 2);
    coy_set_uint_vector(ctx, 1, (const uint32_t[]){10, 12}, 2);
    ASSERT(coy_call(ctx, "main", "add_uint2"));
    vuint = coy_get_uint_vector(ctx, 0, &vsize);
    ASSERT(vuint);
    ASSERT_EQ_INT(vsize, 2);
    ASSERT_EQ_INT(vuint[0], 15);
    ASSERT_EQ_INT(vuint[1], 19);

    coy_env_deinit(&env);
}

int main()
{
    TEST_EXEC(stb_ds);
    TEST_EXEC(lexer);
    TEST_EXEC(parser);
    TEST_EXEC(semantic_analysis);
    TEST_EXEC(typeinfo_integer);
    TEST_EXEC(typeinfo_tensor);
    TEST_EXEC(typeinfo_array);
    TEST_EXEC(typeinfo_function);
    TEST_EXEC(typeinfo_intern_dedup);
    TEST_EXEC(vm_basic);
    TEST_EXEC(codegen);
    TEST_EXEC(function_builder_verify);
    TEST_EXEC(vm_factorial);
    TEST_EXEC(vm_factorial_call);
    TEST_EXEC(vm_native_call);
    TEST_EXEC(vm_native_retcall);
    TEST_EXEC(vm_native_call_direct);
    TEST_EXEC(vm_vector2_add);
    return TEST_REPORT();
}
