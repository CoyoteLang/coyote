#ifndef COY_VM_SLOTS_H_
#define COY_VM_SLOTS_H_

#include "../util/bitarray.h"

struct coy_gc_;

struct coy_slots_
{
    coy_bitarray_t pregs;       //< which registers are pointers
    union coy_register_* regs;
};

struct coy_slots_* coy_slots_init_(struct coy_slots_* slots, size_t initsize);
void coy_slots_deinit_(struct coy_slots_* slots);
void coy_slots_gc_mark_(struct coy_slots_* slots, struct coy_gc_* gc);

void coy_slots_setlen_(struct coy_slots_* slots, size_t nlen);
size_t coy_slots_getlen_(struct coy_slots_* slots);

union coy_register_ coy_slots_get_(struct coy_slots_* slots, size_t i, bool* isptr);
union coy_register_ coy_slots_getval_(struct coy_slots_* slots, size_t i);
void* coy_slots_getptr_(struct coy_slots_* slots, size_t i);
void coy_slots_set_(struct coy_slots_* slots, size_t i, union coy_register_ reg, bool isptr);
void coy_slots_setval_(struct coy_slots_* slots, size_t i, union coy_register_ val);
void coy_slots_setptr_(struct coy_slots_* slots, size_t i, void* ptr);

void coy_slots_copy_(struct coy_slots_* dst, size_t d, struct coy_slots_* src, size_t s);

#endif /* COY_VM_SLOTS_H_ */
