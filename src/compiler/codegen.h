#ifndef COY_CODEGEN_H
#define COY_CODEGEN_H

#include <setjmp.h>
#include "ast.h"
#include "bytecode.h"
#include "vm/env.h"

typedef struct {
    jmp_buf err_env;
    char *err_msg;
    coy_env_t env;
    struct coy_module_ *module;
    struct coy_function_ *functions;
    function_t *func;
    block_t *block;
} coyc_cctx_t;

coyc_cctx_t coyc_codegen(ast_root_t *root);
void coyc_cg_free(coyc_cctx_t ctx);

#endif
