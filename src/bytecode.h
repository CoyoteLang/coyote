#ifndef COY_BYTECODE_H
#define COY_BYTECODE_H

#include <stdint.h>

#include "vm/vm.h"

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
        uint32_t index: 24;
        uint32_t _reserved : 8; // eventually: VM register?
    } arg;
    uint32_t raw;
};

// This can eventually be merged with `instrs`, to avoid multiple allocations.
struct coy_function_block_
{
    uint32_t nparams;   //< # of a parameters in a block
    uint32_t offset;    //< block's starting offset
};

typedef int32_t coy_c_function_t(coy_thread_t* thread);

struct coy_function_
{
    struct coy_function_block_* blocks;    // NULL for native functions
    union
    {
        struct
        {
            union coy_instruction_* instrs;
            uint32_t maxregs;   //< max # of registers used; same as max(block.nparams + block.size - 1)
        } coy;
        coy_c_function_t* cfunc;
    } u;
};

struct coy_module_
{
    const struct coy_refval_vtbl_* vtbl;
    union coy_register_* variables;
};

#endif