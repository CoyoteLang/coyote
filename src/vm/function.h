#ifndef COY_VM_FUNCTION_H_
#define COY_VM_FUNCTION_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define COY_FUNCTION_ATTRIB_NATIVE_ UINT32_C(0x00000001)

struct coy_context;
struct coy_module_;

typedef int32_t coy_c_function_t(struct coy_context* ctx, void* udata);

union coy_instruction_
{
    struct
    {
        uint8_t code;
        uint8_t flags;
        uint8_t _reserved;  // eventually: VM register?
        uint8_t nargs;
    } op;
    struct
    {
        uint32_t index: 23;
        uint32_t _reserved: 8;  // eventually: VM register?
        uint32_t isconst: 1;
    } arg;
    uint32_t raw;
};

// Constants are always stored in the following order (to simplify the linker & GC):
// - symbols
// - reference constants
// - other constants
// Order *within* a group of constants is arbitrary.
struct coy_function_constants_
{
    uint32_t nsymbols;  //< how many constants are symbols?
    uint32_t nrefs;     //< how many constants are references?
    // (remaining constants are simple values)
    union coy_register_* data;  //< constants themselves
};

// this will eventually replace coy_function_
struct coy_function_block_
{
    uint32_t offset;    //< offset to start of block (entry block must start at 0!)
    uint32_t nparams;   //< number of parameters
    uint32_t* ptrs;     //< which registers are pointers?
};
struct coy_function_
{
    const struct coy_typeinfo_* type;   //< this is a function type, *not* return type
    union
    {
        struct
        {
            struct coy_function_constants_ consts;
            struct coy_function_block_* blocks;
            union coy_instruction_* instrs;
            uint32_t maxslots: 31;  //< max number of stack slots used, in any block
            uint32_t is_linked: 1;  //< was the function already linked?
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
struct coy_function_* coy_function_init_empty_(struct coy_function_* func, const struct coy_typeinfo_* type, uint32_t attrib);
struct coy_function_* coy_function_init_data_(struct coy_function_* func, const struct coy_typeinfo_* type, uint32_t attrib, const void* data, size_t datalen);
struct coy_function_* coy_function_init_native_(struct coy_function_* func, const struct coy_typeinfo_* type, uint32_t attrib, coy_c_function_t* handler, void* udata);

void coy_function_coy_compute_maxslots_(struct coy_function_* func);
bool coy_function_verify_(struct coy_function_* func);
bool coy_function_link_(struct coy_function_* func, struct coy_module_* module);

#endif /* COY_VM_FUNCTION_H_ */
