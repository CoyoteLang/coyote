#include "compiler.h"
#include "lexer.h"
#include "ast.h"
#include "semalysis.h"
#include "codegen.h"

#include <string.h>

bool coyc_init(coyc_t *compiler, coy_env_t *env) {
    if (!compiler || !env) {
        return false;
    }
    compiler->env = env;
    return true;
}

bool coyc_compile(coyc_t *compiler, const char *const fname, const char *const src) {
    coyc_pctx_t pctx;
    ast_root_t root;
    coyc_lexer_t lexer;
    pctx.lexer = &lexer;
    pctx.root = &root;
    coyc_lexer_init(&lexer, fname ? fname : "<src>", src, strlen(src) - 1);
    coyc_parse(&pctx);
    if (pctx.err_msg) {
        return false;
    }
    char *smsg = coyc_semalysis(&root);
    if (smsg) {
        return false;
    }
    coyc_cctx_t cctx = coyc_codegen(&root, compiler->env);
    if (cctx.err_msg) {
        return false;
    }

    return true;
}

bool coyc_deinit(coyc_t *compiler) {
    return true;
}