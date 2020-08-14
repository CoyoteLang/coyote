#ifndef COY_SEMALYSIS_H
#define COY_SEMALYSIS_H

#include <setjmp.h>
#include "ast.h"

typedef struct {
    jmp_buf err_env;
    char *err_msg;
} coyc_sctx_t;

/// If non-null, the returned string is caller-owned, and contains an error message
char *coyc_semalysis(ast_root_t *root);

#endif