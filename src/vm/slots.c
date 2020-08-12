#include "slots.h"
#include "register.h"
#include "gc.h"
#include "../util/debug.h"

#include "stb_ds.h"
#include <limits.h>

struct coy_slots_* coy_slots_init_(struct coy_slots_* slots, size_t initsize)
{
    if(!slots) return NULL;
    coy_bitarray_init(&slots->pregs);
    coy_bitarray_setlen(&slots->pregs, initsize);
    slots->regs = NULL;
    stbds_arrsetlen(slots->regs, initsize);
    return slots;
}
void coy_slots_deinit_(struct coy_slots_* slots)
{
    if(!slots) return;
    coy_bitarray_deinit(&slots->pregs);
    stbds_arrfree(slots->regs);
}
void coy_slots_gc_mark_(struct coy_slots_* slots, struct coy_gc_* gc)
{
    // mark all pointer registers
    for(size_t i = 0; i < coy_bitarray_getlen_elems(&slots->pregs); i++)
    {
        size_t pregs = coy_bitarray_get_elem(&slots->pregs, i);
        if(!pregs)  // no pointers in this page
            continue;
        // this could be optimized further at some point (especially if CPU has a find-nonzero-bit instruction!
        for(size_t j = 0; j < sizeof(size_t) * CHAR_BIT; j++)
            if(pregs & ((size_t)1 << j))
                coy_gc_mark_(gc, slots->regs[i * sizeof(size_t) * CHAR_BIT + j].ptr);
    }
}

void coy_slots_setlen_(struct coy_slots_* slots, size_t nlen)
{
    coy_bitarray_setlen(&slots->pregs, nlen);
    stbds_arrsetlen(slots->regs, nlen);
}
size_t coy_slots_getlen_(struct coy_slots_* slots)
{
    return stbds_arrlenu(slots->regs);
}

union coy_register_ coy_slots_get_(struct coy_slots_* slots, size_t i, bool* isptr)
{
    COY_CHECK(i < coy_slots_getlen_(slots));
    if(isptr) *isptr = coy_bitarray_get(&slots->pregs, i);
    return slots->regs[i];
}
union coy_register_ coy_slots_getval_(struct coy_slots_* slots, size_t i)
{
    bool isptr;
    union coy_register_ reg = coy_slots_get_(slots, i, &isptr);
    COY_CHECK_MSG(!isptr, "expected a non-pointer value");
    return reg;
}
void* coy_slots_getptr_(struct coy_slots_* slots, size_t i)
{
    bool isptr;
    union coy_register_ reg = coy_slots_get_(slots, i, &isptr);
    COY_CHECK_MSG(isptr, "expected a pointer value");
    return reg.ptr;
}
void coy_slots_set_(struct coy_slots_* slots, size_t i, union coy_register_ reg, bool isptr)
{
    COY_CHECK(i < coy_slots_getlen_(slots));
    coy_bitarray_set(&slots->pregs, i, isptr);
    slots->regs[i] = reg;
}
void coy_slots_setval_(struct coy_slots_* slots, size_t i, union coy_register_ val)
{
    coy_slots_set_(slots, i, val, false);
}
void coy_slots_setptr_(struct coy_slots_* slots, size_t i, void* ptr)
{
    union coy_register_ reg;
    reg.ptr = ptr;
    coy_slots_set_(slots, i, reg, true);
}
