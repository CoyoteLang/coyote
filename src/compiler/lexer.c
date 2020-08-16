#include "lexer.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <stdio.h>

#define COYC_LEXER_EOF_ -1

coyc_lexer_t* coyc_lexer_init(coyc_lexer_t* lexer, const char* fname, const char* src, size_t srclen)
{
    if(!lexer) return NULL;
    size_t fnlen = strlen(fname);
    char* buf = malloc(fnlen + 1 + srclen + 1); // so that we allocate both in 1 alloc
    lexer->fname = buf;
    memcpy(lexer->fname, fname, fnlen + 1);

    lexer->src = &buf[fnlen + 1];
    lexer->offset = (size_t)-1; // this signifies first iteration
    lexer->srclen = 0;
    // yeah, preprocessing; this is *HORRIBLE*, but I can do it properly later
    for(size_t i = 0; i < srclen; i++)
    {
        if(src[i] == '\r')
        {
            lexer->src[lexer->srclen++] = '\n';
            if(i + 1 < srclen && src[i+1] == '\n')
                ++i;    // jump over another i, to get over the LF of CRLF
        }
        else
            lexer->src[lexer->srclen++] = src[i];
    }
    lexer->src[lexer->srclen] = 0; // not technically necessary, but just for defense in depth

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

static void coyc_lexer_advancec_(coyc_lexer_t* lexer, size_t c)
{
    if(lexer->offset + c >= lexer->srclen)
        c = lexer->srclen - lexer->offset;
    for(size_t i = 0; i < c; i++)
    {
        if(lexer->src[lexer->offset + i] == '\n')
        {
            ++lexer->pos.line;
            lexer->pos.col = 0;
        }
        else
            ++lexer->pos.col;   // ignore UTF-8 for now
    }
    lexer->offset += c;
}
static int coyc_lexer_peekc_(coyc_lexer_t* lexer, size_t c)
{
    c += lexer->offset;
    return c < lexer->srclen ? (uint8_t)lexer->src[c] : COYC_LEXER_EOF_;
}
static void coyc_lexer_skip_until_(coyc_lexer_t* lexer, int until, bool inclusive)
{
    size_t i = 0;
    for(;;)
    {
        int c = coyc_lexer_peekc_(lexer, i);
        if(c < 0 || c == until)
        {
            if(inclusive && c >= 0)
                ++i;
            break;
        }
        ++i;
    }
    coyc_lexer_advancec_(lexer, i);
}

coyc_token_t coyc_lexer_mktoken_(coyc_lexer_t* lexer, coyc_token_kind_t kind, size_t c)
{
    lexer->token.kind = kind;
    coyc_lexer_advancec_(lexer, c);
    lexer->token.len = &lexer->src[lexer->offset] - lexer->token.ptr;
    lexer->token.range.tail = lexer->pos;
    return lexer->token;
}

#define COYC_LEXER_ISKEYWORD_(lexer, keyword) ((lexer)->token.len == strlen(keyword) && !memcmp((lexer)->token.ptr, keyword, strlen(keyword)))

const char *types[] = {
    "int", "uint", "u32"
};

coyc_token_t coyc_lexer_next(coyc_lexer_t* lexer, uint32_t categories)
{
    //if(!categories) categories = COYC_LEXER_CATEGORY_PARSER;
    if(lexer->offset == (size_t)-1)
    {
        lexer->offset = 0;
        // First iteration; not too important otherwise, but I wanted to get it out of the way, lest I forget.
        if(lexer->srclen >= 3 && !memcmp(&lexer->src[lexer->offset], "\xEF\xBB\xBF", 3))
            lexer->offset += 3;  //< skip UTF-8 BOM (TODO: warning?)
        while(lexer->srclen >= 2 && !memcmp(&lexer->src[lexer->offset], "#!", 2))
            coyc_lexer_skip_until_(lexer, '\n', true);  //< skip shebangs
    }
    for(;;)
    {
        lexer->token.kind = COYC_TK_ERROR;
        lexer->token.ptr = &lexer->src[lexer->offset];
        lexer->token.len = 0;
        lexer->token.range.head = lexer->pos;

        size_t i;
        int c = coyc_lexer_peekc_(lexer, 0);
        switch(c)
        {
        case COYC_LEXER_EOF_: return coyc_lexer_mktoken_(lexer, COYC_TK_EOF, 0);
        case '+': return coyc_lexer_mktoken_(lexer, COYC_TK_OPADD, 1);
        case '-': return coyc_lexer_mktoken_(lexer, COYC_TK_OPSUB, 1);
        case '*': return coyc_lexer_mktoken_(lexer, COYC_TK_OPMUL, 1);
        case '/':
            c = coyc_lexer_peekc_(lexer, 1);
            switch(c)
            {
            case '/':
                coyc_lexer_advancec_(lexer, 2);
                coyc_lexer_skip_until_(lexer, '\n', false);
                if(categories & COYC_LEXER_CATEGORY_IGNORABLE)
                    return coyc_lexer_mktoken_(lexer, COYC_TKI_SLCOMMENT, 0);
                break;
            case '*':
                for(i = 2;; i++)
                {
                    c = coyc_lexer_peekc_(lexer, i);
                    if(c == COYC_LEXER_EOF_)
                    {
                        fprintf(stderr, "lexer error: unterminated `/*` comment\n");
                        abort();
                    }
                    if(c == '*' && coyc_lexer_peekc_(lexer, i+1) == '/')
                    {
                        i += 2;
                        break;
                    }
                }
                if(categories & COYC_LEXER_CATEGORY_IGNORABLE)
                    return coyc_lexer_mktoken_(lexer, COYC_TKI_MLCOMMENT, i);
                break;
            default: return coyc_lexer_mktoken_(lexer, COYC_TK_OPDIV, 1);
            }
            break;
        case '.': return coyc_lexer_mktoken_(lexer, COYC_TK_DOT, 1);
        case ',': return coyc_lexer_mktoken_(lexer, COYC_TK_COMMA, 1);
        case ';': return coyc_lexer_mktoken_(lexer, COYC_TK_SCOLON, 1);
        case '(': return coyc_lexer_mktoken_(lexer, COYC_TK_LPAREN, 1);
        case ')': return coyc_lexer_mktoken_(lexer, COYC_TK_RPAREN, 1);
        case '{': return coyc_lexer_mktoken_(lexer, COYC_TK_LBRACE, 1);
        case '}': return coyc_lexer_mktoken_(lexer, COYC_TK_RBRACE, 1);
        // \r should never show up, but just in case
        case '\r': case '\n': case '\t': case '\v': case '\f': case ' ':
            for(i = 1;; i++)
            {
                c = coyc_lexer_peekc_(lexer, i);
                if(c == COYC_LEXER_EOF_ || !strchr("\r\n\t\v\f ", c))
                    break;
            }
            if(categories & COYC_LEXER_CATEGORY_IGNORABLE)
                return coyc_lexer_mktoken_(lexer, COYC_TKI_WSPACE, i);
            coyc_lexer_advancec_(lexer, i);
            break;
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            for(i = 1; c >= '0' && c <= '9'; i++)
                c = coyc_lexer_peekc_(lexer, i);
            return coyc_lexer_mktoken_(lexer, COYC_TK_INTEGER, i - 1);
        // ugh ... but at least it's efficient! (I didn't want to cheat with `default`)
        case '_':
        case 'A': case 'B': case 'C': case 'D': case 'E':
        case 'F': case 'G': case 'H': case 'I': case 'J':
        case 'K': case 'L': case 'M': case 'N': case 'O':
        case 'P': case 'Q': case 'R': case 'S': case 'T':
        case 'U': case 'V': case 'W': case 'X': case 'Y':
        case 'Z':
        case 'a': case 'b': case 'c': case 'd': case 'e':
        case 'f': case 'g': case 'h': case 'i': case 'j':
        case 'k': case 'l': case 'm': case 'n': case 'o':
        case 'p': case 'q': case 'r': case 's': case 't':
        case 'u': case 'v': case 'w': case 'x': case 'y':
        case 'z':
            for(i = 1;; i++)
            {
                c = coyc_lexer_peekc_(lexer, i);
                if(!('0' <= c && c <= '9') && !('A' <= c && c <= 'Z') && !('a' <= c && c <= 'z') && c != '_')
                    break;
            }
            coyc_lexer_mktoken_(lexer, COYC_TK_IDENT, i);
            bool set = false;
            for (size_t j = 0; j < sizeof(types) / sizeof(char*); j += 1) {
                if (COYC_LEXER_ISKEYWORD_(lexer, types[j])) {
                    lexer->token.kind = COYC_TK_TYPE;
                    set = true;
                    break;
                }
            }
            if (set) {}
            else if (COYC_LEXER_ISKEYWORD_(lexer, "module"))
                lexer->token.kind = COYC_TK_MODULE;
            else if(COYC_LEXER_ISKEYWORD_(lexer, "import"))
                lexer->token.kind = COYC_TK_IMPORT;
            else if(COYC_LEXER_ISKEYWORD_(lexer, "native"))
                lexer->token.kind = COYC_TK_NATIVE;
            else if(COYC_LEXER_ISKEYWORD_(lexer, "return"))
                lexer->token.kind = COYC_TK_RETURN;
            return lexer->token;
        default:
            fprintf(stderr, "\nlexer error near '%c' (\\x%.2X)\n", c, c);
            abort();
            break;
        }
    }
    assert(0);
    return lexer->token;
}
