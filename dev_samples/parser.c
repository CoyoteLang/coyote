static coyc_anode_t* coyc_p_expression_(struct coyc_pctx_* pctx)
{
    return coyc_ph_precedence_climb_(pctx, /*int minprec=*/0);
}
static coyc_anode_t* coyc_p_value_(struct coyc_pctx_* pctx)
{
    coyc_anode_t* node;
    // might be better to have some static consts, and do `coyc_pctx_check_first_(coyc_pctx_t* pctx, const coyc_token_kind* firsts)`
    // ... but for the sake of simplicity of example:
    if(pctx->lexer->token.kind == COYC_TK_LPAREN)   // . LPAREN expression RPAREN
    {
        // could wrap this into coyc_pctx_next_, and handle lexer errors gracefully
        coyc_lexer_next(pctx->lexer, COYC_LEXER_CATEGORY_PARSER);   // LPAREN . expression RPAREN
        node = coyc_p_expression_(pctx);
        // NOTE: This is why `token` must be in lexer (it could be in `pctx` instead, too): `coyc_p_expression` will change the current token;
        // so we either need a *pointer to* a token (error-prone to use/update), or just store it in lexer!
        if(pctx->lexer->token.kind != COYC_TK_RPAREN)   // LPAREN expression . RPAREN
            coyc_lexer_next(pctx->lexer, COYC_LEXER_CATEGORY_PARSER);   // LPAREN expression RPAREN .
        // alternative to above:
        //coyc_pctx_next_expect_(pctx, token);
    }
    // the other approach to checks (yes, I know comments between `}` and `else` are bad form, bear with me for the example!)
    else if(coyc_pctx_check_first_(pctx, (const coyc_token_kind[]){COYC_TK_IDENT, COYC_TK_INTEGER}))    // IDENT | INTEGER | ...
    {
        // create a node out of a token ("inheriting" its kind, string, and input range)
        node = coyc_anode_new_token_(pctx->lexer->token);
        coyc_lexer_next(pctx->lexer, COYC_LEXER_CATEGORY_PARSER);
    }
    else if(pctx->lexer->token.kind == COYC_TK_LARRAY)  // array
        node = coyc_p_array_(pctx);
    else
        error_expect((const char*[]){"'('", "identifier", "integer", "array"});  // "expected ${error_expect}, found ${token_name} (${token_string_abbrev})"
}

// called from coyc_parse; this is a normal function
static root_node_t* coyc_p_module_(struct coyc_pctx_* pctx)
{
    // see above for example of content
    ...
}

coyc_anode_t* coyc_parse(coyc_lexer_t* lexer)
{
    struct coyc_pctx_ pctx = {
        // can also hold:
        // - list of all allocated nodes (for cleanup on error)
        // - list of error messages (for now just 1, but if/when we do error recovery, we'll have a list!)
        // - any kind of per-parse debug info
        // - anything else that needs to be stored "globally" per-parse
        .lexer = lexer,
    };
    coyc_lexer_next(lexer, COYC_LEXER_CATEGORY_PARSER); // "prime" the lexer by grabbing 1 token
    // optionally: setup setjmp() and such
    coyc_anode_t* module = coyc_p_module_(&pctx);
    // optionally: handle setjmp() error; but right now, I'll just check `module`
    if(!module)
        error("something failed to parse in coyc_p_module_");
    // this step is required, otherwise we're allowing for trailing garbage!
    if(lexer->token.kind != COYC_TK_EOF)
        error("expected end of file");
    return module;
}
