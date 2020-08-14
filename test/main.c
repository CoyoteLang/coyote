#include "compiler/lexer.h"
#include "compiler/ast.h"
#include "compiler/semalysis.h"
#include "compiler/codegen.h"

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
    ASSERT_EQ_INT(func.return_type.type, primitive);
    ASSERT_EQ_INT(func.return_type.primitive.primitive, _int);
    ASSERT(func.statements);
    ASSERT_EQ_UINT(arrlen(func.statements), 1);
    statement_t stmt = func.statements[0];
    ASSERT_EQ_INT(stmt.type, return_);
    ASSERT(stmt.return_.value);
    ASSERT_EQ_INT(stmt.return_.value->lhs.type, literal);
    ASSERT(stmt.return_.value->lhs.type == literal);
    // If rhs.type is none, the value is just the lhs, and op is 
    ASSERT_EQ_INT(stmt.return_.value->rhs.type, none);
    ASSERT_EQ_INT(stmt.return_.value->type.type, primitive);
    ASSERT_EQ_INT(stmt.return_.value->type.primitive.primitive, int_literal);
    ASSERT_EQ_INT(stmt.return_.value->lhs.literal.value.integer.type.type, primitive);
    ASSERT_EQ_INT(stmt.return_.value->lhs.literal.value.integer.type.primitive.primitive, int_literal);
    ASSERT_EQ_INT(stmt.return_.value->lhs.literal.value.integer.value, 0);

    // Finally, clean up. Note that I only do this so Valgrind doesn't complain;
    // this will be handled by the OS anyways.
    coyc_tree_free(&ctx);
    coyc_lexer_deinit(&lexer);
}

extern void vm_test_basicDBG(void);
TEST(vm_basic)
{
    vm_test_basicDBG();
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
    ASSERT_TODO("REIMPLEMENT");
    //ASSERT(sctx);
    //ASSERT_EQ_STR(sctx->err_msg, NULL);
    //ASSERT(sctx->module);
    //ASSERT(sctx->module->functions);
    coy_env_t env;
    coy_env_init(&env);

    //coy_context_t* ctx = coy_context_create(&env);
    //coy_context_push_frame_(ctx, &sctx->module->functions[0], 4, false);
    //coy_push_uint(ctx, 2);
    //coy_push_uint(ctx, 3);
    //coy_push_uint(ctx, 1);
    //coy_push_uint(ctx, 24);
    //coy_vm_exec_frame_(ctx);

    //ASSERT_EQ_UINT(coy_slots_getval_(&ctx->top->slots, 0).u32, 0);

    //coy_env_deinit(&env);   //< not yet implemented
}

const char *bad_srcs[] = {
    "module 1;",
    "module test;\n"
    "module 2;",
    "int a() {}",
};

const char *bad_parse_msgs[] = {
    "Expected identifier for module name!",
    "Duplicate module statement found!",
    "Missing a module statement!",
};

const char *bad_comp_msgs[] = {
    NULL,
    NULL,
    NULL,
};

coy_function_t good_funcs[] = {
    { .blocks = NULL, },
    { .blocks = NULL, },
    { .blocks = NULL, },
};

TEST(semantic_analysis) {
    size_t src_size = sizeof(bad_srcs) / sizeof(*bad_srcs);
    size_t pmsg_size = sizeof(bad_parse_msgs) / sizeof(*bad_parse_msgs);
    size_t smsg_size = sizeof(bad_comp_msgs) / sizeof(*bad_comp_msgs);
    size_t gmsg_size = sizeof(good_funcs) / sizeof(coy_function_t);
    PRECONDITION(src_size == pmsg_size && "Forgot to add a parser error message (or NULL)");
    PRECONDITION(src_size == smsg_size && "Forgot to add a compilation error message (or NULL)");
    PRECONDITION(src_size == gmsg_size && "Forgot to add a coy_function_t ");
    for (size_t i = 0; i < sizeof(bad_srcs) / sizeof(*bad_srcs); i += 1) {
        PRECONDITION(bad_srcs[i]);
        coyc_pctx_t pctx;
        ast_root_t root;
        coyc_lexer_t lexer;
        PRECONDITION(coyc_lexer_init(&lexer, "<semalysis_test>", bad_srcs[i], strlen(bad_srcs[i])));
        pctx.lexer = &lexer;
        pctx.root = &root;
        coyc_parse(&pctx);
        ASSERT_EQ_STR(pctx.err_msg, bad_parse_msgs[i]);
        if (!pctx.err_msg) {
            // Parser is good, let's check semalysis
            ASSERT_TODO("IMPLEMENT");
        //    coyc_sctx_t *sctx = coyc_semalysis(&root);
//            ASSERT(sctx);
  //          ASSERT_EQ_STR(sctx->err_msg, bad_comp_msgs[i]);
        }
        else {
            PRECONDITION(!bad_comp_msgs[i]);
        }
    }
}

TEST(vm_jmpc)
{
/*
uint factorial(uint num, uint const1=1)
{
    uint acc = const1;
    while(num > 0)
    {
        acc = acc * num;
        num = num - const1;
    }
    return acc;
}
----------
u32 factorial(u32 num, u32 const1)
.0_entry(num,const1):
    jmp .1_test($0, $1, $1)
.1_test(num,const1,acc):
    $3 = sub $1, $1         ; ... we need a `0` ...
    jmpc lt $3, $0,         ;0 < num
        .2_loop($0,$1,$2),
        .3_end($2)
.2_loop(num,const1,acc):
    $3 = mul $2, $0
    $6 = sub $0, $1
    jmp .1_test($6, $1, $3)
.3_end(acc):
    ret $0
*/

/*
jmpc encoding for `jmpc lt $a, $b, .X($1,$2), .Y($3,$4)`:
    OP:     op=jmpc flags=TYPE_x | CMP_LT               ; CMP is one of EQ, NE, LT, LE
    ARG:    reg=$a
    ARG:    reg=$b
    ARG:    immediate=.X                                ; encoded directly (not a reg)
    ARG:    immediate=.Y                                ; encoded directly (not a reg)
    ARG:    immediate=moves_sep                         ; end of .X, start of .Y
    ARG:    <arbitrary amount of pairs of moves>
    ; .X's parameters are in args: [5,5+moves_sep)
    ; .Y's parameters are in args: [5+moves_sep,nargs)
*/

    static const struct coy_function_block_ in_blocks[] = {
        {2,0},  //entry
        {3,4},  //test
        {3,15}, //loop
        {1,27}, //end
    };
    static const union coy_instruction_ in_instrs[] = {
        //.0_entry(2)   @0
        /*-*/   {.op={COY_OPCODE_JMP, 0, 0, 3}},
        /*-*/   {.raw=1},
        /*-*/   {.arg={2,0}},
        /*-*/   {.arg={1,0}},

        //.1_test(3)    @4
        /*3*/   {.op={COY_OPCODE_SUB, COY_OPFLG_TYPE_UINT32, 0, 2}},
        /*4*/   {.arg={1,0}},
        /*5*/   {.arg={1,0}},
        /*-*/   {.op={COY_OPCODE_JMPC, COY_OPFLG_TYPE_UINT32|COY_OPFLG_CMP_LT, 0, 7}},
        /*-*/   {.arg={3,0}},
        /*-*/   {.arg={0,0}},
        /*-*/   {.raw=2},
        /*-*/   {.raw=3},
        /*-*/   {.raw=0},
        /*-*/   {.arg={0,0}},
        /*-*/   {.arg={2,0}},

        //.2_loop(3)    @15
        /*3*/   {.op={COY_OPCODE_MUL, COY_OPFLG_TYPE_UINT32, 0, 2}},
        /*4*/   {.arg={2,0}},
        /*5*/   {.arg={0,0}},
        /*6*/   {.op={COY_OPCODE_SUB, COY_OPFLG_TYPE_UINT32, 0, 2}},
        /*7*/   {.arg={0,0}},
        /*8*/   {.arg={1,0}},
        /*-*/   {.op={COY_OPCODE_JMP, 0, 0, 5}},
        /*-*/   {.raw=1},
        /*-*/   {.arg={0,0}},
        /*-*/   {.arg={6,0}},
        /*-*/   {.arg={2,0}},
        /*-*/   {.arg={3,0}},

        //.3_end(1)     @27
        /*-*/   {.op={COY_OPCODE_RET, 0, 0, 1}},
        /*-*/   {.arg={0,0}},

/*
u32 factorial(u32 num, u32 const1)
.0_entry(num,const1):
    jmp .1_test($0, $1, $1)
.1_test(num,const1,acc):
    $3 = sub $1, $1         ; ... we need a `0` ...
    jmpc lt $3, $0,         ;0 < num
        .2_loop($0,$1,$2),
        .3_end($2)
.2_loop(num,const1,acc):
    $3 = mul $2, $0
    $6 = sub $0, $1
    jmp .1_test($6, $1, $3)
.3_end(acc):
    ret $0
*/
    };

    struct coy_function_ func = {NULL};
    stbds_arrsetlen(func.blocks, sizeof(in_blocks) / sizeof(*in_blocks));
    memcpy(func.blocks, in_blocks, sizeof(in_blocks));
    stbds_arrsetlen(func.u.coy.instrs, sizeof(in_instrs) / sizeof(*in_instrs));
    memcpy(func.u.coy.instrs, in_instrs, sizeof(in_instrs));
    coy_function_update_maxregs_(&func);

    coy_env_t env;
    coy_env_init(&env);

    coy_context_t* ctx = coy_context_create(&env);
    coy_context_push_frame_(ctx, &func, 2, false);
    coy_push_uint(ctx, 6);
    coy_push_uint(ctx, 1);
    coy_vm_exec_frame_(ctx);

    printf("RESULT: %" PRIu32 "\n", coy_slots_getval_(&ctx->top->slots, 0).u32);

    //coy_env_deinit(&env);   //< not yet implemented
}

int main()
{
    TEST_EXEC(lexer);
    TEST_EXEC(parser);
    TEST_EXEC(semantic_analysis);
    TEST_EXEC(vm_basic);
    TEST_EXEC(vm_jmpc);
    TEST_EXEC(codegen);
    return TEST_REPORT();
}
