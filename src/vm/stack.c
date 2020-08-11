#include "stack.h"
#include "gc.h"
#include "typeinfo.h"
#include "context.h"
#include "register.h"

#include "stb_ds.h"
#include <limits.h>

#define COY_STACK_INITIAL_REG_SIZE_      2048
#define COY_STACK_INITIAL_FRAME_SIZE_    16

static void coy_mark_stack_segment_(struct coy_gc_* gc, void* ptr)
{
    struct coy_stack_segment_* seg = ptr;
    // mark all pointer registers
    for(size_t i = 0; i < coy_bitarray_getlen_elems(&seg->pregs); i++)
    {
        size_t pregs = coy_bitarray_get_elem(&seg->pregs, i);
        if(!pregs)  // no pointers in this page
            continue;
        // this could be optimized further at some point (especially if CPU has a find-nonzero-bit instruction!
        for(size_t j = 0; j < sizeof(size_t) * CHAR_BIT; j++)
            if(pregs & ((size_t)1 << j))
                coy_gc_mark_(gc, seg->regs[i * sizeof(size_t) * CHAR_BIT + j].ptr);
    }
    // TODO: Mark all of frames[i].function?
}
static void coy_dtor_stack_segment_(struct coy_gc_* gc, void* ptr)
{
    // TODO: Add an assertion to ensure that this is not in thread->top.
    struct coy_stack_segment_* seg = ptr;
    coy_bitarray_deinit(&seg->pregs);
    stbds_arrfree(seg->regs);
    stbds_arrfree(seg->frames);
}
static const struct coy_typeinfo_ coy_ti_stack_segment_ = {
    .category = COY_TYPEINFO_CAT_INTERNAL_,
    .u={.internal_name = "stack_segment"},
    .cb_mark = coy_mark_stack_segment_,
    .cb_dtor = coy_dtor_stack_segment_,
};
struct coy_stack_segment_* coy_stack_segment_create_(struct coy_context* ctx)
{
    struct coy_stack_segment_* seg = coy_gc_malloc_(&ctx->gc, sizeof(struct coy_stack_segment_), &coy_ti_stack_segment_);
    seg->parent = ctx->top;
    coy_bitarray_init(&seg->pregs);
    seg->regs = NULL;
    stbds_arrsetcap(seg->regs, COY_STACK_INITIAL_REG_SIZE_);
    seg->frames = NULL;
    stbds_arrsetcap(seg->frames, COY_STACK_INITIAL_FRAME_SIZE_);
    return seg;
}
