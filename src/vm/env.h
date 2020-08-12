#ifndef COY_VM_ENV_H_
#define COY_VM_ENV_H_

#include <stdint.h>

struct coy_context;
struct coy_module_entry_;

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
} coy_env_t;

coy_env_t* coy_env_init(coy_env_t* env);

#endif /* COY_VM_ENV_H_ */
