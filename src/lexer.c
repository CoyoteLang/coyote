#include "lexer.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

coyc_lexer_t* coyc_lexer_init(coyc_lexer_t* lexer, const char* fname, const char* src, size_t srclen)
{
    if(!lexer) return NULL;
    size_t fnlen = strlen(fname);
    char* buf = malloc(fnlen + 1 + srclen + 1); // so that we allocate both in 1 alloc
    lexer->fname = buf;
    memcpy(lexer->fname, fname, fnlen + 1);
    lexer->srclen = srclen;
    lexer->src = &buf[fnlen + 1];
    memcpy(lexer->src, src, srclen);
    lexer->src[srclen] = 0; // not technically necessary, but just for defense in depth
    lexer->pos = (coy_source_pos_t){.line=0, .col=0};
    lexer->token = (coyc_token_t){.kind=COYC_TK_ERROR};
    return lexer;
}
void coyc_lexer_deinit(coyc_lexer_t* lexer)
{
    if(!lexer) return;
    free(lexer->fname);
    //lexer->src is in the same allocation, so we mustn't free it
}

coyc_token_t* coyc_lexer_next(coyc_lexer_t* lexer, uint32_t categories)
{
    if(!categories) categories = COYC_LEXER_CATEGORY_PARSER;
    assert(0 && "TODO");
    return NULL;
}
