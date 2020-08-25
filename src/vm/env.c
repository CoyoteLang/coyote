#include "env.h"
#include "context.h"
#include "function.h"
#include "../util/string.h"
#include "../util/debug.h"

#include "stb_ds.h"
#include <stdlib.h>
#include <string.h>

enum coy_module_validation_status_
{
    COY_MODULE_VALIDATION_INVALID_,
    COY_MODULE_VALIDATION_OK_,
    COY_MODULE_VALIDATION_RESERVED_,
};
static enum coy_module_validation_status_ coy_module_validate_name_(const char* name)
{
    /*
        RESERVED.* is reserved by VM. Eventually, this'll hold the actual name.
        TODO: Figure out the name. Options:
        - core.*
        - lang.*
        - coyote.*
        - system.*
        - runtime.*
        - some keyword, unusable in Coyote (e.g. "default.*" or even "$internal")
    */
    static const char ReservedPackage[] = "RESERVED";

    const char* segstart = name;
    bool firstchar = true;
    for(const char* ptr = name;; ptr++)
    {
        if(*ptr == '_'
        || ('A' <= *ptr && *ptr <= 'Z')
        || ('a' <= *ptr && *ptr <= 'z')
        || (!firstchar && '0' <= *ptr && *ptr <= '9'))
        {
            // okay; do nothing
        }
        else if(*ptr == '.' || !*ptr)
        {
            size_t seglen = ptr - segstart;
            if(seglen == 0)     //< empty segment (e.g. "foo..bar")
                return COY_MODULE_VALIDATION_INVALID_;
            if(segstart == name
            && seglen == sizeof(ReservedPackage) - 1u
            && !memcmp(ReservedPackage, segstart, seglen))    //< reserved first name
                return COY_MODULE_VALIDATION_RESERVED_;
            if(!*ptr)   // if this was the last segment, we're done
                break;
            firstchar = true;
        }
        else    //< invalid character for identifier
            return COY_MODULE_VALIDATION_INVALID_;
    }
    return COY_MODULE_VALIDATION_OK_;
}
struct coy_module_* coy_module_create_(struct coy_env* env, const char* name, bool allow_reserved)
{
    enum coy_module_validation_status_ validation = coy_module_validate_name_(name);
    if(!COY_ENSURE(validation, "misuse: invalid module name"))
        return NULL;
    if(validation == COY_MODULE_VALIDATION_RESERVED_ && !allow_reserved)
        return NULL;    //< error: module uses reserved package (this is handled outside of the `ensure` because a reserved name is not an *API* misuse)
    if(stbds_shgetp_null(env->modules, name))
        return NULL;    //< error: module already exists
    struct coy_module_* module = malloc(sizeof(struct coy_module_));
    module->env = env;
    module->name = coy_strdup_(name, -1);
    module->symbols = NULL;
    stbds_shput(env->modules, module->name, module);
    return module;
}

void coy_module_inject_function_(struct coy_module_* module, const char* name, struct coy_function_* function)
{
#if 0   // stb_ds bug
    struct coy_module_symbol_entry_* entry = stbds_shgetp_null(module->symbols, name);
#else
    ptrdiff_t entryidx = stbds_shgeti(module->symbols, name);
    struct coy_module_symbol_entry_* entry = entryidx >= 0 ? &module->symbols[entryidx] : NULL;
#endif
    if(entry)
    {
        if(!COY_ENSURE(entry->value.category == COY_MODULE_SYMCAT_FUNCTION_, "only functions can be overloaded"))
            return;
        COY_TODO("function overloads");
    }
    else
    {
        struct coy_module_symbol_ sym;
        sym.name = coy_strdup_(name, -1);
        sym.category = COY_MODULE_SYMCAT_FUNCTION_;
        sym.u.functions = NULL;
        stbds_arrput(sym.u.functions, function);
        stbds_shput(module->symbols, sym.name, sym);
    }
}
bool coy_module_link_(struct coy_module_* module)
{
    bool ok = true;
    for(size_t s = 0; s < stbds_shlenu(module->symbols); s++)
    {
        const struct coy_module_symbol_* sym = &module->symbols[s].value;
        // we only link functions (skip others)
        if(sym->category != COY_MODULE_SYMCAT_FUNCTION_)
            continue;
        for(size_t f = 0; f < stbds_arrlenu(sym->u.functions); f++)
            if(!coy_function_link_(sym->u.functions[f], module))
                ok = false;
    }
    return ok;
}
struct coy_module_symbol_* coy_module_find_symbol_(struct coy_module_* module, const char* name)
{
#if 0   // stb_ds bug
    struct coy_module_symbol_entry_* entry = stbds_shgetp_null(module->symbols, name);
#else
    ptrdiff_t entryidx = stbds_shgeti(module->symbols, name);
    struct coy_module_symbol_entry_* entry = entryidx >= 0 ? &module->symbols[entryidx] : NULL;
#endif
    return entry ? &entry->value : NULL;
}

coy_env_t* coy_env_init(coy_env_t* env)
{
    if(!env) return NULL;
    env->contexts.ptr = NULL;
    env->contexts.next_id = 1;
    env->modules = NULL;
    env->typeinfos = NULL;
    return env;
}
void coy_env_deinit(coy_env_t* env)
{
    if(!env) return;
    while(stbds_arrlenu(env->contexts.ptr))
        coy_context_destroy(env->contexts.ptr[0]);
    stbds_shfree(env->modules);
}

struct coy_module_* coy_env_find_module_(coy_env_t* env, const char* name)
{
#if 0   // stb_ds bug
    struct coy_module_entry_* entry = stbds_shgetp_null(env->modules, name);
#else
    ptrdiff_t entryidx = stbds_shgeti(env->modules, name);
    struct coy_module_entry_* entry = entryidx >= 0 ? &env->modules[entryidx] : NULL;
#endif
    return entry ? entry->value : NULL;
}
