#ifndef COY_SEMALYSIS_H
#define COY_SEMALYSIS_H

#include <setjmp.h>
#include "ast.h"

/// If non-null, the returned string is caller-owned, and contains an error message
char *coyc_semalysis(ast_root_t *root);

#endif
