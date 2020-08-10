#ifndef COY_VM_H_
#define COY_VM_H_

#include <stddef.h>
#include <stdint.h>

// Information for the garbage collector. This will be optimized at some point, but for now, let's do the easy thing.
struct coy_gcobj_;

#define COY_GC_SET_ROOT_    2
#define COY_GC_SET_COMMON_  3
struct coy_gc_
{
    struct coy_gcobj_** sets[3];
    uint8_t curset : 1;
    uint8_t : 7;
};

struct coy_stack_segment_;

// A thread is store specific to a particular thread.
// The intended use is for programs that need multiple contexts to reuse coy_vm, but create a new thread per context.
// TODO: Rename to coy_context? But I suspect it might get confused with coy_vm.
//       Or perhaps coy_heap would be a good name (because we use a 1:1:1 mapping between threads, heaps, and GC instances, anyways).
typedef struct coy_thread
{
    struct coy_vm* vm;
    struct coy_gc_ gc;
    uint32_t index;                 //< thread index in thread list; not unique
    uint32_t id;                    //< thread ID; unique for a particular execution
    struct coy_stack_segment_* top; //< top (current) stack segment
} coy_thread_t;

typedef int32_t coy_c_function_t(coy_thread_t* thread);

struct coy_module_entry_;

// A VM is a shared context that stores immutable data such as code.
typedef struct coy_vm
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

coy_vm_t* coy_vm_init(coy_vm_t* vm);
coy_thread_t* coy_vm_create_thread(coy_vm_t* vm);
coy_thread_t* coy_vm_get_thread(coy_vm_t* vm, size_t i);

#endif /* COY_VM_H_ */
