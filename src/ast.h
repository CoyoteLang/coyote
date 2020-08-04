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
root_node_t *coyc_parse(coyc_lexer_t *lexer, root_node_t *root);
void coyc_tree_free(root_node_t *node);

#endif // COY_AST_H_

