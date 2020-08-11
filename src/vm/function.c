#include "function.h"
#include "../util/debug.h"

#include "stb_ds.h"
#include <stddef.h>

void coy_function_update_maxregs_(struct coy_function_* func)
{
    if(!func->blocks) return;   // it's a native function, so we have nothing to do
    uint32_t maxregs = 0;
    size_t nblocks = stbds_arrlenu(func->blocks);
    for(size_t b = 0; b < nblocks; b++)
    {
        struct coy_function_block_* block = &func->blocks[b];
        uint32_t blen = (b + 1 < nblocks ? func->blocks[b+1].offset : stbds_arrlenu(func->u.coy.instrs)) - block->offset;
        COY_ASSERT(blen);   // empty blocks should not exist
        blen += block->nparams; // effective length is increased by this
        if(blen > maxregs)
            maxregs = blen;
    }
    func->u.coy.maxregs = maxregs;
}
