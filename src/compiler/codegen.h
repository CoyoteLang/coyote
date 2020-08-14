#ifndef COY_CODEGEN_H
#define COY_CODEGEN_H

#include <setjmp.h>
#include "ast.h"

typedef struct {
    jmp_buf err_env;
    char *err_msg;
} coyc_cctx_t;

coyc_cctx_t coyc_codegen(ast_root_t *root);

#endif
