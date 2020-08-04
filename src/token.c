#include "token.h"

// for debug
#include <stdio.h>
#include <inttypes.h>

void coyc_dumpstr_escaped_DBG(const char* str, size_t len)
{
    putchar('`');
    while(len--)
    {
        uint8_t c = *str++;
        switch(c)
        {
        case '\r': fputs("\\r", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        case '\v': fputs("\\v", stdout); break;
        case '\f': fputs("\\f", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '`': fputs("\\`", stdout); break;
        default:
            if(c < ' ' || 0x80 <= c)
                printf("\\x%.2X", c);
            else
                putchar(c);
            break;
        }
    }
    putchar('`');
}
void coyc_token_dump_DBG(const coyc_token_t* token)
{
    printf("%3" PRIu32 ":%s(%" PRIu32 ":%" PRIu32 "): ", token->kind, coyc_token_kind_tostr_DBG(token->kind), token->range.head.line, token->range.head.col);
    coyc_dumpstr_escaped_DBG(token->ptr, token->len);
}
const char* coyc_token_kind_tostr_DBG(coyc_token_kind_t kind)
{
    static const char* KindNames[COYC_TK_LAST_+1] = {
#define COYC_ITEM_(NAME)    [COYC_TK_##NAME] = #NAME
#define COYC_ITEMI_(NAME)   [COYC_TKI_##NAME] = "!" #NAME
#define COYC_VAL_(V)
#define COYC_VLAST_(V)
        COYC_ENUM_token_kind(COYC_ITEM_,COYC_ITEMI_,COYC_VAL_,COYC_VLAST_)
#undef COYC_ITEM_
#undef COYC_ITEMI_
#undef COYC_VAL_
#undef COYC_VLAST_
    };
    if(kind < 0 || sizeof(KindNames) / sizeof(*KindNames) <= kind)
        return "<unknown>";
    return KindNames[kind];
}