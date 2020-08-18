// Compatibility shims; ideally, this entire file will be gone at some point.
#ifndef COY_COMPAT_SHIMS_H_
#define COY_COMPAT_SHIMS_H_

#include "context.h"
#include "env.h"
#include "stack.h"
#include "register.h"

#include <stdint.h>

// temporary APIs
struct coy_context;
void coy_push_uint(struct coy_context* ctx, uint32_t u);

#endif /* COY_COMPAT_SHIMS_H_ */
