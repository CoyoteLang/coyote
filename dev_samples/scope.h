typedef struct symtab symtab_t;  //< placeholder for symbol table

enum coyc_scope_type_
{
    COYC_STYPE_DECL,    // declarative scope; two-pass parsing, order irrelevant
    COYC_STYPE_FUNC,    // function scope; same as block, but "terminates" shadowing detection
    COYC_STYPE_BLOCK,   // block scope
};

struct coyc_scope_
{
    coyc_anode_t* owner;
    struct coyc_scope* parent;

    symtab_t* imports;
    symtab_t* locals;
    // optional, if you want to avoid looking up parent
    symtab_t* imports_all;
    symtab_t* locals_all;
};