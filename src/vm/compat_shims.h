// Compatibility shims; ideally, this entire file will be gone at some point.
#ifndef COY_COMPAT_SHIMS_H_
#define COY_COMPAT_SHIMS_H_

#include "gc.h"
#include "typeinfo.h"
#include "context.h"
#include "env.h"
#include "stack.h"
#include "compat_function.h"
#include "register.h"
#include "slots.h"

#include <stddef.h>
#include <stdint.h>
#include "../util/bitarray.h"

// these are internal, which is why they end with '_' and had *no* typedef
typedef union coy_instruction_ coy_instruction_t;
typedef struct coy_function_block_ coy_function_block_t;
typedef struct coy_compat_function_ coy_function_t;

// temporary APIs
void coy_push_uint(coy_context_t* ctx, uint32_t u);
void coy_vm_exec_frame_(coy_context_t* ctx);

#endif /* COY_COMPAT_SHIMS_H_ */
