#ifndef COY_AST_H_
#define COY_AST_H_

#include <stdbool.h>
#include <stdint.h>

#include "lexer.h"

typedef struct import {
    char *name;
} import_t;

typedef struct expression expression_t;
struct expression {
    expression_t *children;
};

typedef union returnv {
    uint64_t constant;
} return_t;

typedef union {
    return_t return_value;
} instruction_t;

typedef enum primitive {
    invalid, uint, _int,
} primitive_t;

typedef union type {
    primitive_t primitive;
} type_t;

typedef struct function {
    char *name;
    instruction_t *instructions;
    type_t return_type;
} function_t;

typedef struct {
    char *module_name;
    import_t *imports;
    function_t *functions;
} root_node_t;

/// Returns the root node
#include <setjmp.h>

typedef struct coyc_pctx {
    root_node_t *root;
    coyc_token_t *tokens;
    size_t token_index;
    char *err_msg;
    coyc_lexer_t *lexer;
    jmp_buf err_env;
} coyc_pctx_t;

// Pointer can get clobbered by longjmp, so we don't return it.
void coyc_parse(coyc_pctx_t *ctx);
void coyc_tree_free(coyc_pctx_t *ctx);

#endif // COY_AST_H_

