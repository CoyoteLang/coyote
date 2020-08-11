#ifndef COY_VM_H_
#define COY_VM_H_

#include "gc.h"
#include "typeinfo.h"
#include "context.h"
#include "env.h"

#include <stddef.h>
#include <stdint.h>
#include "../util/bitarray.h"

struct coy_stack_segment_;

// compatibility shims
typedef struct coy_context coy_thread_t;
//typedef struct coy_env coy_vm_t;

struct coy_module_entry_;

// A VM is a shared context that stores immutable data such as code.
typedef __attribute__((deprecated)) struct coy_vm
{
    struct
    {
        //uint32_t mem;
        //uint32_t len;
        coy_thread_t** ptr;
        uint32_t next_id;
    } threads;
    struct coy_module_entry_* modules;
} coy_vm_t;

// A stack segment is a contiguous segment of registers.
struct coy_stack_segment_
{
    struct coy_stack_segment_* parent;
    coy_bitarray_t pregs;   //< which registers are pointers
    union coy_register_* regs;
    // TODO: frames could eventually be merged into `regs`, with a clever use of the stack
    // (but this is easier for debugging)
    struct coy_frame_* frames;
};

coy_vm_t* coy_vm_init(coy_vm_t* vm);
coy_thread_t* coy_vm_create_thread(coy_vm_t* vm);
coy_thread_t* coy_vm_get_thread(coy_vm_t* vm, size_t i);

#endif /* COY_VM_H_ */
