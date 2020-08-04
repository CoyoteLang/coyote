#include "lexer.h"
#include "ast.h"
#include "stb_ds.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// a simple assert won't do, because those are disableable
#define CHECK(x)    do { if(!(x)) { fprintf(stderr, "\x1b[31mTest failed: %s\x1b[0m\n", #x); failures += 1; } else { printf("\x1b[32mTest '%s' passed.\n\x1b[0m", #x); } } while(0)

int main()
{
    // First, test the lexer
    static const char src[] =
            "module test;\n"
            // Need to decide how these are handled internally first.
            // "import std.io;\n"
            "int foo()\n"
            "{\n"
            "    return 0;\n"
            "}\n"
    ;

    size_t failures = 0;

    coyc_lexer_t lexer;
    CHECK(coyc_lexer_init(&lexer, "<src>", src, sizeof(src) - 1));

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

    // Now, test the parser
    // The lexer is empty now - so we relex
    coyc_lexer_deinit(&lexer);
    CHECK(coyc_lexer_init(&lexer, "<src>", src, sizeof(src) - 1));

    root_node_t root;
    CHECK(coyc_parse(&lexer, &root) == &root);

    // Validate the results
    CHECK(root.module_name);
    CHECK(strcmp(root.module_name, "test") == 0);
    CHECK(!root.imports);
    CHECK(root.functions);
    CHECK(arrlen(root.functions) == 1);
    function_t func = root.functions[0];
    CHECK(func.name);
    CHECK(strcmp(func.name, "foo") == 0);
    CHECK(func.return_type.primitive == _int);
    CHECK(func.instructions);
    CHECK(arrlen(func.instructions) == 1);
    CHECK(func.instructions[0].return_value.constant == 0);

    if (failures == 0) {
        printf("All tests passed.\n");
    }
    else {
        printf("\n\x1b[31mTests failed!\n\x1b[0m");
    }

    // Finally, clean up. Note that I only do this so Valgrind doesn't complain;
    // this will be handled by the OS anyways.
    coyc_tree_free(&root);
    coyc_lexer_deinit(&lexer);
    return failures;
}
