#include "context.h"
#include "env.h"
#include "stack.h"
#include "../util/debug.h"

#include "stb_ds.h"

coy_context_t* coy_context_create(struct coy_env* env)
{
    coy_context_t* ctx = malloc(sizeof(coy_context_t));
    ctx->env = env;
    coy_gc_init_(&ctx->gc);
    ctx->index = stbds_arrlenu(env->contexts.ptr);
    ctx->id = env->contexts.next_id++;
    //ctx->top = coy_context_create_stack_segment_(ctx);
    return stbds_arrput(env->contexts.ptr, ctx);
}

void coy_context_push_frame_(coy_context_t* ctx, struct coy_function_* function, uint32_t nparams, bool segmented)
{
    if(!ctx->top || segmented)
        ctx->top = coy_stack_segment_create_(ctx);
    struct coy_stack_segment_* seg = ctx->top;

    struct coy_stack_frame_ frame;
    frame.fp = stbds_arrlenu(seg->regs);
    frame.bp = frame.fp + nparams;
    frame.block = 0;
    frame.pc = 0;
    frame.function = function;
    stbds_arrput(seg->frames, frame);
}
void coy_context_pop_frame_(coy_context_t* ctx)
{
    COY_ASSERT(ctx->top);
    struct coy_stack_segment_* seg = ctx->top;
    size_t nframes = stbds_arrlenu(seg->frames);
    COY_CHECK(nframes);
    stbds_arrsetlen(seg->frames, nframes - 1);
    if(nframes == 1 && seg->parent)    // if we had 1 frame earlier, then the entire stack segment can be destroyed
        ctx->top = seg->parent;
}
