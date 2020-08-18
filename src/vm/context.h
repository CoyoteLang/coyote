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
    struct coy_slots_ slots;        //< slots for native<->Coyote calls, and as a scratch buffer
} coy_context_t;

coy_context_t* coy_context_create(struct coy_env* env);
void coy_context_destroy(coy_context_t* ctx);

void coy_context_push_frame_(coy_context_t* ctx, struct coy_function_* function, bool segmented);
void coy_context_pop_frame_(coy_context_t* ctx);

uint32_t coy_num_slots(coy_context_t* ctx);
void coy_ensure_slots(coy_context_t* ctx, uint32_t nslots);
int32_t coy_normalize_index(coy_context_t* ctx, int32_t index);

uint32_t coy_get_uint(coy_context_t* ctx, int32_t index);
void coy_set_uint(coy_context_t* ctx, int32_t index, uint32_t value);

bool coy_call(coy_context_t* ctx, const char* module_name, const char* function_name);

#endif /* COY_VM_CONTEXT_H_ */
