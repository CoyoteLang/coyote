#ifndef COY_LEXER_H_
#define COY_LEXER_H_

#include "token.h"

#include <stddef.h>

typedef struct coyc_lexer
{
    char* fname;
    size_t srclen;
    char* src;          //< we'll just copy it into this (again, KISS)
    size_t offset;
    coy_source_pos_t pos;
    coyc_token_t token;
} coyc_lexer_t;

coyc_lexer_t* coyc_lexer_init(coyc_lexer_t* lexer, const char* fname, const char* src, size_t srclen);
void coyc_lexer_deinit(coyc_lexer_t* lexer);

// NOTE: don't depend on the actual category values being stable
#define COYC_LEXER_CATEGORY_PARSER      0x00    //< default, and unignorable
#define COYC_LEXER_CATEGORY_IGNORABLE   0x80
coyc_token_t* coyc_lexer_next(coyc_lexer_t* lexer, uint32_t categories);

#endif /* COY_LEXER_H_ */
