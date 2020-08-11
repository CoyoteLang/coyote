#ifndef COY_COMPAT_SHIMS_H_
#define COY_COMPAT_SHIMS_H_

#include "gc.h"
#include "typeinfo.h"
#include "context.h"
#include "env.h"
#include "stack.h"
#include "function.h"
#include "register.h"

#include <stddef.h>
#include <stdint.h>
#include "../util/bitarray.h"

// compatibility shims
typedef union coy_instruction_ coy_instruction_t;
typedef struct coy_function_block_ coy_function_block_t;
typedef struct coy_function_ coy_function_t;

void coy_push_uint(coy_context_t* ctx, uint32_t u);
void coy_vm_exec_frame_(coy_context_t* ctx);

#endif /* COY_COMPAT_SHIMS_H_ */
