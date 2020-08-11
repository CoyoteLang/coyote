#include "env.h"

#include <stddef.h>

coy_env_t* coy_env_init(coy_env_t* env)
{
    if(!env) return NULL;
    env->contexts.ptr = NULL;
    env->contexts.next_id = 1;
    env->modules = NULL;
    return env;
}
