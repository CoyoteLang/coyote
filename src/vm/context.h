#ifndef COY_VM_CONTEXT_H_
#define COY_VM_CONTEXT_H_

#include "gc.h"
#include "slots.h"

#include <stdbool.h>

struct coy_env;
struct coy_function_;

// A thread is store specific to a particular thread.
// The intended use is for programs that need multiple contexts to reuse coy_vm, but create a new thread per context.
// TODO: Rename to coy_context? But I suspect it might get confused with coy_vm.
//       Or perhaps coy_heap would be a good name (because we use a 1:1:1 mapping between threads, heaps, and GC instances, anyways).
typedef struct coy_context
{
    struct coy_env* env;
    struct coy_gc_ gc;
    uint32_t index;                 //< thread index in thread list; not unique
    uint32_t id;                    //< thread ID; unique for a particular execution
    struct coy_stack_segment_* top; //< top (current) stack segment
    struct coy_slots_ nslots;       //< slots for native<->Coyote calls
} coy_context_t;

coy_context_t* coy_context_create(struct coy_env* env);

void coy_context_push_frame_(coy_context_t* ctx, struct coy_function_* function, uint32_t nparams, bool segmented);
void coy_context_pop_frame_(coy_context_t* ctx);

#endif /* COY_VM_CONTEXT_H_ */
