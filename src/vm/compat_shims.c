#include "compat_shims.h"
#include "context.h"

void coy_push_uint(struct coy_context* ctx, uint32_t u)
{
    struct coy_stack_segment_* seg = ctx->top;
    coy_slots_setlen_(&seg->slots, coy_slots_getlen_(&seg->slots) + 1);
    union coy_register_ val = {.u32 = u};
    coy_slots_setval_(&seg->slots, coy_slots_getlen_(&seg->slots) - 1, val);
}
