#ifndef COY_VM_COMPAT_FUNCTION_H_
#define COY_VM_COMPAT_FUNCTION_H_

// need coy_instruction_, coy_function_block_, and a few other things from it
#include "function.h"

struct coy_compat_function_
{
    struct coy_function_block_* blocks;    // NULL for native functions
    union
    {
        struct
        {
            union coy_instruction_* instrs;
            uint32_t maxregs;   //< max # of registers used; same as max(block.nparams + block.size)
        } coy;
        coy_c_function_t* cfunc;
    } u;
};

void coy_function_update_maxregs_(struct coy_compat_function_* func);

#endif /* COY_VM_COMPAT_FUNCTION_H_ */
