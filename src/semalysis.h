#ifndef COY_SEMALYSIS_H
#define COY_SEMALYSIS_H

#include <setjmp.h>

#include "bytecode.h"
#include "ast.h"

typedef enum {
    param, func, var
} scope_entry_type_t;

typedef union {
   scope_entry_type_t type;
   struct {
       scope_entry_type_t type;
       parameter_t param;
   } param;
} scope_entry_t;

typedef struct {
    scope_entry_t *entries;
} scope_t;

typedef struct {
    jmp_buf err_env;
    char *err_msg;
    coy_module_t *module;
    scope_t scope;
    // Indices are owned by the function_t
    char **indices;
    size_t *regs;
} coyc_sctx_t;

coyc_sctx_t *coyc_semalysis(ast_root_t *root);

#endif