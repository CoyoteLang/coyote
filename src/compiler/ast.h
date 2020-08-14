#ifndef COY_AST_H_
#define COY_AST_H_

#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>

#include "lexer.h"

typedef enum primitive {
    invalid, uint, _int, int_literal,
} primitive_t;

typedef enum {
    no_type, primitive, 
} type_type_t;

typedef union type {
    type_type_t type;
    struct {
        type_type_t type;
        primitive_t primitive;
    } primitive;
} type_t;

typedef struct {
    enum {function, import} type;
    char *name;
} decl_base_t;

typedef enum {
    return_
} statement_type_t;

typedef struct expression expression_t;

typedef enum {
    none, literal, identifier, parameter, expression,  
} expression_value_type_t;

typedef union {
    struct {
        type_t type;
        // Bitcast this to the correct type
        uint64_t value;
    } integer;
} literal_t;

typedef union {
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
} expression_value_t;

struct expression {
    type_t type;
    coyc_token_t op;
    expression_value_t lhs, rhs;
};

typedef union {
    statement_type_t type;
    struct {
        statement_type_t type;
        expression_t *value;
    } return_;
} statement_t;

typedef struct {
    type_t type;
    char *name;
} parameter_t;

typedef struct function {
    decl_base_t base;
    type_t return_type;
    statement_t *statements;
    parameter_t *parameters;
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

