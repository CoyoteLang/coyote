#ifndef COY_TOKEN_H_
#define COY_TOKEN_H_

#include <stdint.h>

typedef struct coy_source_pos
{
    uint32_t line, col;
} coy_source_pos_t;
typedef struct coy_source_range
{
    coy_source_pos_t head, tail;
} coy_source_range_t;

typedef enum coyc_token_kind
{
    // I intend to actually use a macro trick here, for debugging!
    COYC_TK_ERROR = -1,
    COYC_TK_EOF = 0,    //< also used for errors

    // ignorable tokens (whitespace, comments, ...)
    COYC_TKI_WSPACE,
    COYC_TK_NUM_
} coyc_token_kind_t;
const char* coyc_token_kind_tostr_DBG(coyc_token_kind_t* kind);

typedef struct coyc_token
{
    coyc_token_kind_t kind;
    uint32_t len;   // really don't need 64 bits!
    char* ptr;
    coy_source_range_t pos; // it's a bit wasteful to keep an entire range, but it'll aid debugging
} coyc_token_t;

#endif /* COY_TOKEN_H_ */
