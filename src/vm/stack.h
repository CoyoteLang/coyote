#ifndef COY_VM_STACK_H_
#define COY_VM_STACK_H_

#include "../util/bitarray.h"

#include <stdint.h>

struct coy_context;

// A stack frame is a single function context, storing any information necessary for its execution.
struct coy_stack_frame_
{
    // "frame pointer"; points to location on stack of register $0
    // all reads are offset by `fp`
    uint32_t fp;
    // "base pointer", or `fp+nparams`; points to location on stack of first instruction's result
    // all writes are offset by `bp`
    uint32_t bp;
    // "block index"; which block are we in?
    uint32_t block;
    // "program counter"
    uint32_t pc;
    struct coy_function_* function;
};

struct coy_stack_segment_
{
    struct coy_stack_segment_* parent;
    coy_bitarray_t pregs;   //< which registers are pointers
    union coy_register_* regs;
    // TODO: frames could eventually be merged into `regs`, with a clever use of the stack
    // (but this is easier for debugging)
    struct coy_stack_frame_* frames;
};

struct coy_stack_segment_* coy_stack_segment_create_(struct coy_context* ctx);

#endif /* COY_VM_STACK_H_ */
