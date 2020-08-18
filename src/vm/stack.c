#include "stack.h"
#include "gc.h"
#include "../typeinfo.h"
#include "context.h"
#include "register.h"

#include "stb_ds.h"
#include <limits.h>

#define COY_STACK_INITIAL_REG_SIZE_      2048
#define COY_STACK_INITIAL_FRAME_SIZE_    16

static void coy_mark_stack_segment_(struct coy_gc_* gc, void* ptr)
{
    struct coy_stack_segment_* seg = ptr;
    coy_slots_gc_mark_(&seg->slots, gc);
}
static void coy_dtor_stack_segment_(struct coy_gc_* gc, void* ptr)
{
    // TODO: Add an assertion to ensure that this is not in thread->top.
    struct coy_stack_segment_* seg = ptr;
    coy_slots_deinit_(&seg->slots);
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
    coy_slots_init_(&seg->slots, COY_STACK_INITIAL_REG_SIZE_);
    seg->frames = NULL;
    stbds_arrsetcap(seg->frames, COY_STACK_INITIAL_FRAME_SIZE_);
    return seg;
}
