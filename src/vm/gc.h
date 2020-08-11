#ifndef COY_VM_GC_H_
#define COY_VM_GC_H_

#include <stddef.h>
#include <stdint.h>

// Information for the garbage collector. This will be optimized at some point, but for now, let's do the easy thing.
struct coy_gcobj_;
struct coy_typeinfo_;

#define COY_GC_SET_ROOT_    2
#define COY_GC_SET_COMMON_  3
struct coy_gc_
{
    struct coy_gcobj_** sets[3];
    uint8_t curset : 1;
    uint8_t : 7;
};

struct coy_gc_* coy_gc_init_(struct coy_gc_* gc);
void* coy_gc_malloc_(struct coy_gc_* gc, size_t size, const struct coy_typeinfo_* typeinfo);
void coy_gc_mark_(struct coy_gc_* gc, void* ptr);

#endif /* COY_VM_GC_H_ */
