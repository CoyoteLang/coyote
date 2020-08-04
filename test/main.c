#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>

// a simple assert won't do, because those are disableable
#define CHECK(x)    do { if(!(x)) { fprintf(stderr, "CHECK failed: CHECK(%s)\n", #x); exit(1); } } while(0)

int main()
{
    static const char src[] =
            "import std.io;\n"
            "int foo()\n"
            "{\n"
            "    return 0;\n"
            "}\n"
    ;

    coyc_lexer_t lexer;
    CHECK(coyc_lexer_init(&lexer, "<src>", src, sizeof(src) - 1));

    for(;;)
    {
        coyc_token_t* token = coyc_lexer_next(&lexer, COYC_LEXER_CATEGORY_PARSER);
        if(token->kind == COYC_TK_EOF)
            break;
        coyc_token_dump_DBG(token);
        fputs("\n", stdout);
        fflush(stdout);
        if(token->kind == COYC_TK_ERROR)
            break;
    }

    coyc_lexer_deinit(&lexer);
	return 0;
}
