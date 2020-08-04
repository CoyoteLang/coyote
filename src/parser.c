/**
 Implementation notes

 This is currently not error-tolerant, at all. Post-jam, making the parser fully
tolerant is extremely important, and a high-priority TODO.
 */

#include "ast.h"
#include "stb_ds.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

root_node_t *error(root_node_t *root, const char *msg) {
    fprintf(stderr, "Parser: %s\n", msg);
    if (root->module_name) {
        free(root->module_name);
        root->module_name = NULL;
    }
    if (root->imports) {
        // TODO
    }
    return NULL;
}

type_t coyc_type(coyc_token_t token) {
    type_t type;
    type.primitive = invalid;
    if (strncmp(token.ptr, "int", token.len) == 0) {
        type.primitive = _int;
    }
    return type;
}

root_node_t *coyc_parse(coyc_lexer_t *lexer, root_node_t *root)
{
    if (!lexer) return NULL;
    if (!root) return NULL;

    root->imports = NULL;
    root->module_name = NULL;
    root->functions = NULL;

    coyc_token_t token = coyc_lexer_next(lexer, COYC_LEXER_CATEGORY_PARSER);
    if (token.kind != COYC_TK_MODULE) {
        return error(root, "Missing a module statement!");
    }

    char buf[256];
    
    while (token.kind != COYC_TK_EOF && token.kind != COYC_TK_ERROR)
    {
        switch (token.kind)
        {
        case COYC_TK_MODULE:
            if (root->module_name)
            {
                return error(root, "Invalid `module` statement found!");
            }
            token = coyc_lexer_next(lexer, COYC_LEXER_CATEGORY_PARSER);
            if (token.kind != COYC_TK_IDENT) {
                return error(root, "Expected identifier for module name!");
            }
            root->module_name = malloc(token.len + 1);
            strncpy(root->module_name, token.ptr, token.len);
            root->module_name[token.len] = 0;

            token = coyc_lexer_next(lexer, COYC_LEXER_CATEGORY_PARSER);
            if (token.kind != COYC_TK_SCOLON) {
                return error(root, "Expected semicolon after module statement!");
            }

            break;
        case COYC_TK_TYPE:{
            const type_t type = coyc_type(token);
            if (type.primitive == invalid) {
                buf[0] = 0;
                strncat(strcat(buf, "TODO parse type "), token.ptr, token.len);
                return error(root, buf);
            }
            token = coyc_lexer_next(lexer, COYC_LEXER_CATEGORY_PARSER);
            switch (token.kind)
            {
            case COYC_TK_IDENT: {
                char *name = malloc(token.len + 1);
                name[token.len] = 0;
                strncpy(name, token.ptr, token.len);

                token = coyc_lexer_next(lexer, COYC_LEXER_CATEGORY_PARSER);
                if (token.kind == COYC_TK_LPAREN) {

                    function_t function;
                    function.instructions = NULL;
                    function.name = name;
                    function.return_type = type;
                    
                    token = coyc_lexer_next(lexer, COYC_LEXER_CATEGORY_PARSER);
                    if (token.kind != COYC_TK_RPAREN) {
                        return error(root, "TODO support parsing parameter list");
                    }
                    token = coyc_lexer_next(lexer, COYC_LEXER_CATEGORY_PARSER);
                    if (token.kind != COYC_TK_LBRACE) {
                        return error(root, "Error: expected function body!");
                    }
                    while (true)
                    {
                        token = coyc_lexer_next(lexer, COYC_LEXER_CATEGORY_PARSER);
                        if (token.kind == COYC_TK_EOF) {
                            return error(root, "Error: expected '}', found EOF!");
                        }
                        if (token.kind == COYC_TK_ERROR) {
                            return error(root, "Error in lexer while parsing function body!");
                        }
                        if (token.kind == COYC_TK_RBRACE) {
                            break;
                        }
                        if (token.kind == COYC_TK_RETURN) {
                            token = coyc_lexer_next(lexer, COYC_LEXER_CATEGORY_PARSER);
                            if (token.kind == COYC_TK_INTEGER) {
                                instruction_t instruction;
                                sscanf(token.ptr, "%" SCNu64, &instruction.return_value.constant);
                                arrput(function.instructions, instruction);
                                token = coyc_lexer_next(lexer, COYC_LEXER_CATEGORY_PARSER);
                                if (token.kind != COYC_TK_SCOLON) {
                                    return error(root, "TODO return INT [^;]");
                                }
                            }
                            else {
                                sprintf(buf, "TODO parser.c:%d", __LINE__);
                                return error(root, buf);
                            }
                        }
                        else {
                            sprintf(buf, "TODO parse function instruction %s", coyc_token_kind_tostr_DBG(token.kind));
                            return error(root, buf);
                        }
                        arrput(root->functions, function);
                    }
                }
                else {
                    sprintf(buf, "TODO parser pattern TYPE IDENT %s", coyc_token_kind_tostr_DBG(token.kind));
                    return error(root, buf);
                }
                break; }
            default: 
                sprintf(buf, "TODO parser pattern TYPE %s", coyc_token_kind_tostr_DBG(token.kind));
                return error(root, buf);
            }

            break;
        }
        default:{
            sprintf(buf, "TODO parse token %s", coyc_token_kind_tostr_DBG(token.kind));
            return error(root, buf);
        }}
        token = coyc_lexer_next(lexer, COYC_LEXER_CATEGORY_PARSER);
    }

    return root;
}

void coyc_tree_free(root_node_t *node)
{
    if (node->module_name) {
        free(node->module_name);
        node->module_name = NULL;
    }
    if (node->functions) {
        for (int i = 0; i < arrlen(node->functions); i++) {
            function_t *func = &node->functions[i];
            if (func->name) {
                free(func->name);
                func->name = NULL;
            }
            if (func->instructions) {
                arrfree(func->instructions);
            }
        }
        arrfree(node->functions);
    }
}
