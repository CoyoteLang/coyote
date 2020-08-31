#ifndef COY_COMPILER
#define COY_COMPILER

#include <stdbool.h>

#include "vm/env.h"

typedef struct coyc {
    coy_env_t *env;
} coyc_t;

/// Initializes the compiler with the given environment, which should be preinitialized with coy_env_init.
bool coyc_init(coyc_t *compiler, coy_env_t *env);
/// Returns true upon success. Upon failure, errors can be found in the context.
/// The compiler context must be initialized with coyc_init before compiling.
/// fname is the (optional) name of the file containing the given source. If NULL, `<src>` will be used.
bool coyc_compile(coyc_t *compiler, const char *const fname, const char *const src);
/// Cleans up after the compiler. 
bool coyc_deinit(coyc_t *compiler);

#endif // COY_COMPILER