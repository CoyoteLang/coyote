#include "lexer.h"
#include "ast.h"
#include "semalysis.h"
#include "stb_ds.h"

// TEMPORARY
#include "vm/vm.inl"

#include "test.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
    ASSERT_EQ_INT(func.return_type.primitive, _int);
    ASSERT(func.statements);
    ASSERT_EQ_UINT(arrlen(func.statements), 1);
    statement_t stmt = func.statements[0];
    ASSERT_EQ_INT(stmt.type, return_);
    ASSERT(stmt.return_.value);
    ASSERT_EQ_INT(stmt.return_.value->lhs.type, literal);
    ASSERT(stmt.return_.value->lhs.type == literal);
    // If rhs.type is none, the value is just the lhs, and op is 
    ASSERT_EQ_INT(stmt.return_.value->rhs.type, none);
    ASSERT_EQ_INT(stmt.return_.value->type.primitive, uint);
    ASSERT_EQ_INT(stmt.return_.value->lhs.literal.value.integer.type.primitive, uint);
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

// TODO these are PRIVATE!!!
void coy_push_uint(coy_thread_t* thread, uint32_t u);
void coy_thread_create_frame_(coy_thread_t* thread, struct coy_function_* function, uint32_t nparams, bool segmented);
void coy_thread_exec_frame_(coy_thread_t* thread);

TEST(semalysis)
{ 
    const char src[] =
        "module main;"
        "int foo(int a, int b, int c, int d)\n"
        "{\n"
        // 2, 3, 1, 24
        "    return (((a * b)) / c) - ((d / a) / a);\n"
        "}\n";
        // 9 - (8 / 3)
    coyc_pctx_t ctx;
    ast_root_t root;
    coyc_lexer_t lexer;
    PRECONDITION(coyc_lexer_init(&lexer, "<src>", src, sizeof(src) - 1));
    ctx.lexer = &lexer;
    ctx.root = &root;
    coyc_parse(&ctx);
    ASSERT_EQ_STR(ctx.err_msg, NULL);
    coyc_sctx_t *sctx = coyc_semalysis(&root);
    ASSERT(sctx);
    ASSERT_EQ_STR(sctx->err_msg, NULL);
    ASSERT(sctx->module);
    ASSERT(sctx->module->functions);
    coy_vm_t vm;
    coy_vm_init(&vm);

    coy_thread_t* thread = coy_vm_create_thread(&vm);
    coy_thread_create_frame_(thread, &sctx->module->functions[0], 4, false);
    coy_push_uint(thread, 3);
    coy_push_uint(thread, 3);
    coy_push_uint(thread, 1);
    coy_push_uint(thread, 24);
    coy_thread_exec_frame_(thread);

    ASSERT_EQ_UINT(thread->top->regs[0].u32, 0);

    //coy_vm_deinit(&vm);   //< not yet implemented
}

int main()
{
    TEST_EXEC(lexer);
    TEST_EXEC(parser);
    TEST_EXEC(vm_basic);
    TEST_EXEC(semalysis);
    return TEST_REPORT();
}
