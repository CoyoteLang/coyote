#include "lexer.h"
#include "ast.h"
#include "stb_ds.h"

#include "test.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// a simple assert won't do, because those are disableable
#define CHECK(x)    do { if(!(x)) { fprintf(stderr, "\x1b[31mTest failed: %s\x1b[0m\n", #x); failures += 1; } else { printf("\x1b[32mTest '%s' passed.\n\x1b[0m", #x); } } while(0)

static const char src_lexer_parser[] =
        "module test;\n"
        // Need to decide how these are handled internally first.
        // "import std.io;\n"
        "int foo()\n"
        "{\n"
        "    return 0;\n"
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
    ASSERT_EQ_INT(stmt.return_.value->op.kind, COYC_TK_SCOLON);
    ASSERT_EQ_INT(stmt.return_.value->rhs.type, none);
    ASSERT_EQ_INT(stmt.return_.value->type.primitive, uint);
    ASSERT_EQ_INT(stmt.return_.value->lhs.literal.value.integer.type.primitive, uint);
    ASSERT_EQ_INT(stmt.return_.value->lhs.literal.value.integer.value, 0);

    // Finally, clean up. Note that I only do this so Valgrind doesn't complain;
    // this will be handled by the OS anyways.
    coyc_tree_free(&ctx);
    coyc_lexer_deinit(&lexer);
}

int main()
{
    TEST_EXEC(lexer);
    TEST_EXEC(parser);
    return TEST_REPORT();
}
