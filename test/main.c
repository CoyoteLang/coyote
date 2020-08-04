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

TEST(lexer)
{
    coyc_lexer_t lexer;
    PRECONDITION(coyc_lexer_init(&lexer, "<src_lexer_parser>", src_lexer_parser, sizeof(src_lexer_parser) - 1));
    for(;;)
    {
        coyc_token_t token = coyc_lexer_next(&lexer, COYC_LEXER_CATEGORY_PARSER);
        if(token.kind == COYC_TK_EOF)
            break;
        coyc_token_dump_DBG(&token);
        fputs("\n", stdout);
        fflush(stdout);
        if(token.kind == COYC_TK_ERROR)
            break;
    }
    coyc_lexer_deinit(&lexer);
}

TEST(parser)
{
    coyc_lexer_t lexer;
    PRECONDITION(coyc_lexer_init(&lexer, "<src_lexer_parser>", src_lexer_parser, sizeof(src_lexer_parser) - 1));

    root_node_t root;
    ASSERT_EQ_PTR(coyc_parse(&lexer, &root), &root);

    // Validate the results
    ASSERT(root.module_name);
    ASSERT_EQ_STR(root.module_name, "test");
    ASSERT(!root.imports);
    ASSERT(root.functions);
    ASSERT_EQ_UINT(arrlen(root.functions), 1);
    function_t func = root.functions[0];
    ASSERT(func.name);
    ASSERT_EQ_STR(func.name, "foo");
    ASSERT_EQ_INT(func.return_type.primitive, _int);
    ASSERT(func.instructions);
    ASSERT_EQ_UINT(arrlen(func.instructions), 1);
    ASSERT_EQ_INT(func.instructions[0].return_value.constant, 0);

    // Finally, clean up. Note that I only do this so Valgrind doesn't complain;
    // this will be handled by the OS anyways.
    coyc_tree_free(&root);
    coyc_lexer_deinit(&lexer);
}

int main()
{
    TEST_EXEC(lexer);
    TEST_EXEC(parser);
    return TEST_REPORT();
}
