#ifndef COY_VM_ENV_H_
#define COY_VM_ENV_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

struct coy_env;
struct coy_context;
struct coy_module_symbol_entry_;
struct coy_module_entry_;
struct coy_typeinfo_;

// TODO: I really dislike how this essentially duplicates type info; but it's kind of necessary for function overloading. Taking ideas; the only idea I have is to make the whole thing a multiset, but only allow multiple values for functions.
enum coy_module_symbol_category_
{
    COY_MODULE_SYMCAT_FUNCTION_
};
struct coy_module_symbol_
{
    char* name;
    // static vs instance; immutable (e.g. function declarations) vs mutable (e.g. variables)
    enum coy_module_symbol_category_ category;
    union
    {
        // this is an array due to overloads
        struct coy_function_** functions;
        // example of how it might look like for a variable
        struct
        {
            const struct coy_typeinfo_* type;
            uint32_t slot;  //< index into array of variables (per-context arrays)
        } variable;
    } u;
};
struct coy_module_symbol_entry_
{
    char* key;
    struct coy_module_symbol_ value;
};

struct coy_module_
{
    struct coy_env* env;
    char* name;
    // TODO: this should probably be shared with the compiler's symbol table?
    struct coy_module_symbol_entry_* symbols;
};
struct coy_module_entry_
{
    char* key;
    struct coy_module_* value;
};
struct coy_typeinfo_entry_
{
    char* key;
    struct coy_typeinfo_* value;
};

struct coy_module_* coy_module_create_(struct coy_env* env, const char* name, bool allow_reserved);

void coy_module_inject_function_(struct coy_module_* module, const char* name, struct coy_function_* function);
bool coy_module_link_(struct coy_module_* module);
struct coy_module_symbol_* coy_module_find_symbol_(struct coy_module_* module, const char* name);

// A VM is a shared context that stores immutable data such as code.
typedef struct coy_env
{
    struct
    {
        //uint32_t mem;
        //uint32_t len;
        struct coy_context** ptr;
        uint32_t next_id;
    } contexts;
    struct coy_module_entry_* modules;
    // these are interned
    struct coy_typeinfo_entry_* typeinfos;
} coy_env_t;

coy_env_t* coy_env_init(coy_env_t* env);
void coy_env_deinit(coy_env_t* env);

struct coy_module_* coy_env_find_module_(coy_env_t* env, const char* name);

#endif /* COY_VM_ENV_H_ */
