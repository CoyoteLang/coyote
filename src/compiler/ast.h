#ifndef COY_AST_H_
#define COY_AST_H_

#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>

#include "lexer.h"
#include "../typeinfo.h"

typedef struct {
    enum {function, import} type;
    char *name;
} decl_base_t;

typedef struct expression expression_t;

typedef enum {
    // TODO: splat literal -> int_literal, bool_literal, etc (saves memory,
    // improves performance)
    none, literal, identifier, parameter, expression, call,
} expression_value_type_t;

typedef union {
    struct {
        // Bitcast this to the correct type
        // TODO: 64-bit support
        uint32_t value;
    } integer;
} literal_t;

typedef union expression_value expression_value_t;

union expression_value {
    expression_value_type_t type;
    struct {
        expression_value_type_t type; 
        expression_t *expression;
    } expression;
    struct {
        expression_value_type_t type;
        literal_t value;
    } literal;
    struct {
        expression_value_type_t type;
        char *name;
    } identifier;
    struct {
        expression_value_type_t type;
        size_t index;
    } parameter;
    struct {
        expression_value_type_t type;
        char *name;
        expression_value_t *arguments;
    } call;
};

typedef enum {
    OP_NONE, OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_CALL,
    OP_CMPLT, OP_CMPGT,
} op_type_t;

struct expression {
    struct coy_typeinfo_ type;
    op_type_t op;
    expression_value_t lhs, rhs;
};

typedef enum {
    // Return statement
    return_,
    // Raw expression - e.g. function call, discarded math (`3 + 4`), etc
    expr,
    // Conditional expression
    conditional,
} statement_type_t;

typedef struct block block_t;

typedef union {
    statement_type_t type;
    struct {
        statement_type_t type;
        expression_t *value;
    } expr;
    struct {
        statement_type_t type;
        expression_t *condition;
        /// Blocks owned by function_t
        uint32_t true_block, false_block;
    } conditional;
} statement_t;

typedef struct {
    struct coy_typeinfo_ type;
    char *name;
} parameter_t;

struct block {
    parameter_t *parameters;
    statement_t *statements;
};

typedef struct function {
    decl_base_t base;
    struct coy_typeinfo_ type;
    struct coy_typeinfo_ return_type;
    block_t *blocks;
    block_t *active_block;
} function_t;

typedef struct import {
    decl_base_t base;
} import_t;

// TODO: if decls are order-independent, we can optimize this
typedef union {
    // This is *always* valid; all entries have a base at the beginning
    decl_base_t base;
    import_t import;
    function_t function;
} decl_t;

typedef struct {
    char *module_name;
    decl_t *decls;
} ast_root_t;

typedef struct coyc_pctx {
    ast_root_t *root;
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

