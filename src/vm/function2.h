#ifndef COY_BYTECODE_FILE_H_
#define COY_BYTECODE_FILE_H_

// we need struct coy_instruction_ from it
#include "function.h"

#include <stdbool.h>
#include <stddef.h>

#define COY_FUNCTION_ATTRIB_NATIVE_ UINT32_C(0x00000001)

// Constants are always stored in the following order (to simplify the linker & GC):
// - symbols
// - reference constants
// - other constants
// Order *within* a group of constants is arbitrary.
struct coy_function2_constants_
{
    uint32_t nsymbols;  //< how many constants are symbols?
    uint32_t nrefs;     //< how many constants are references?
    // (remaining constants are simple values)
    union coy_register_* data;  //< constants themselves
};

// this will eventually replace coy_function_
struct coy_function2_block_
{
    uint32_t offset;    //< offset to start of block (entry block must start at 0!)
    uint32_t nparams;   //< number of parameters
    uint32_t* ptrs;     //< which registers are pointers?
};
struct coy_function2_
{
    const struct coy_typeinfo_* type;   //< this is a function type, *not* return type
    union
    {
        struct
        {
            struct coy_function2_constants_ consts;
            struct coy_function2_block_* blocks;
            union coy_instruction_* instrs;
            uint32_t maxslots;  //< max number of stack slots used, in any block
        } coy;
        struct
        {
            coy_c_function_t* handler;
            // user data (we had the space in the union, so might as well)
            void* udata;
        } nat;
    } u;
    uint32_t attrib;
};

// NOTE: `data` is assumed to be uint64_t-aligned
struct coy_function2_* coy_function_init_empty_(struct coy_function2_* func, const struct coy_typeinfo_* type, uint32_t attrib);
struct coy_function2_* coy_function_init_data_(struct coy_function2_* func, const struct coy_typeinfo_* type, uint32_t attrib, const void* data, size_t datalen);
struct coy_function2_* coy_function_init_native_(struct coy_function2_* func, const struct coy_typeinfo_* type, uint32_t attrib, coy_c_function_t* handler, void* udata);

void coy_function_coy_compute_maxslots_(struct coy_function2_* func);
bool coy_function_verify_(struct coy_function2_* func);

#endif /* COY_BYTECODE_FILE_H_ */
