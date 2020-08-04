#ifndef COY_TOKEN_H_
#define COY_TOKEN_H_

#include <stdint.h>
#include <stddef.h>

typedef struct coy_source_pos
{
    uint32_t line, col;
} coy_source_pos_t;
typedef struct coy_source_range
{
    coy_source_pos_t head, tail;
} coy_source_range_t;

// sorry, but we need all the debugging aid we can get!
#define COYC_ENUM_token_kind(ITEM,ITEMI,VAL,VLAST)    \
    ITEM(EOF),              \
    \
    ITEM(DOT),              \
    ITEM(COMMA),            \
    ITEM(SCOLON),           \
    ITEM(LPAREN),           \
    ITEM(RPAREN),           \
    ITEM(LBRACE),           \
    ITEM(RBRACE),           \
    \
    ITEM(TYPE),             \
    ITEM(MODULE),           \
    ITEM(IMPORT),           \
    ITEM(NATIVE),           \
    \
    ITEM(INTEGER),          \
    ITEM(IDENT),            \
    ITEM(RETURN),           \
    \
    ITEMI(WSPACE),          \
    ITEMI(SLCOMMENT),       \
    ITEMI(MLCOMMENT),       \
    \
    ITEM(ERROR)VAL(0xFF)    \
    VLAST(0xFF)

typedef enum coyc_token_kind
{
#define COYC_ITEM_(NAME)    COYC_TK_##NAME
#define COYC_ITEMI_(NAME)   COYC_TKI_##NAME
#define COYC_VAL_(V)        = (V)
#define COYC_VLAST_(V)      ,COYC_TK_LAST_ = (V)
    COYC_ENUM_token_kind(COYC_ITEM_,COYC_ITEMI_,COYC_VAL_,COYC_VLAST_)
#undef COYC_ITEM_
#undef COYC_ITEMI_
#undef COYC_VAL_
#undef COYC_VLAST_
} coyc_token_kind_t;

typedef struct coyc_token
{
    coyc_token_kind_t kind;
    uint32_t len;   // really don't need 64 bits!
    char* ptr;
    coy_source_range_t range; // it's a bit wasteful to keep an entire range, but it'll aid debugging
} coyc_token_t;

// debugging
void coyc_dumpstr_escaped_DBG(const char* str, size_t len);
void coyc_token_dump_DBG(const coyc_token_t* token);
const char* coyc_token_kind_tostr_DBG(coyc_token_kind_t kind);

#endif /* COY_TOKEN_H_ */
