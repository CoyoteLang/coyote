#include "context.h"
#include "env.h"
#include "stack.h"
#include "function.h"
#include "register.h"
#include "vm.h"
#include "../util/debug.h"

#include "stb_ds.h"
#include <string.h>

coy_context_t* coy_context_create(struct coy_env* env)
{
    coy_context_t* ctx = malloc(sizeof(coy_context_t));
    ctx->env = env;
    coy_gc_init_(&ctx->gc);
    ctx->index = stbds_arrlenu(env->contexts.ptr);
    ctx->id = env->contexts.next_id++;
    ctx->top = NULL;
    ctx->top = coy_stack_segment_create_(ctx);
    coy_slots_init_(&ctx->slots, 0);
    return stbds_arrput(env->contexts.ptr, ctx);
}
void coy_context_destroy(coy_context_t* ctx)
{
    if(!ctx) return;
    coy_gc_deinit_(&ctx->gc);
    coy_slots_deinit_(&ctx->slots);
    uint32_t index = ctx->index;
    stbds_arrdelswap(ctx->env->contexts.ptr, index);
    if(stbds_arrlenu(ctx->env->contexts.ptr))
        ctx->env->contexts.ptr[index]->index = index;
}

void coy_context_push_frame_(coy_context_t* ctx, struct coy_function_* function, bool segmented, bool return_native)
{
    if(!ctx->top || segmented)
        ctx->top = coy_stack_segment_create_(ctx);
    struct coy_stack_segment_* seg = ctx->top;

    struct coy_stack_frame_ frame;
    uint32_t nframes = stbds_arrlenu(seg->frames);
    if(nframes && !segmented)
    {
        const struct coy_stack_frame_* pframe = &seg->frames[nframes-1u];
        frame.fp = pframe->bp + (pframe->pc - pframe->function->u.coy.blocks[pframe->block].offset);
    }
    else
        frame.fp = 0;

    frame.bp = frame.fp + function->u.coy.blocks[0].nparams;
    frame.block = 0;
    frame.return_native = return_native;
    frame.pc = 0;
    frame.function = function;
    stbds_arrput(seg->frames, frame);
    coy_slots_setlen_(&seg->slots, frame.fp + function->u.coy.maxslots);
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
    // update slots back to old value
    nframes = stbds_arrlenu(ctx->top->frames);
    if(nframes)
    {
        struct coy_stack_frame_* frame = &ctx->top->frames[nframes-1];
        struct coy_function_* function = frame->function;
        if(!(function->attrib & COY_FUNCTION_ATTRIB_NATIVE_))
            coy_slots_setlen_(&seg->slots, frame->fp + function->u.coy.maxslots);
    }
}

uint32_t coy_num_slots(coy_context_t* ctx)
{
    return coy_slots_getlen_(&ctx->slots);
}
void coy_ensure_slots(coy_context_t* ctx, uint32_t nslots)
{
    coy_slots_setlen_(&ctx->slots, nslots);
}
int32_t coy_normalize_index(coy_context_t* ctx, int32_t index)
{
    // TODO: Should we have this feature?
    //if(index < 0)   //< if index<0, count from the end
    //    index = coy_slots_getlen_(&ctx->slots) + index;
    if(!COY_ENSURE(0 <= index && (size_t)index < coy_slots_getlen_(&ctx->slots), "slot index out of bounds"))
        return 0;
    return index;
}

uint32_t coy_get_uint(coy_context_t* ctx, int32_t index)
{
    return coy_slots_getval_(&ctx->slots, coy_normalize_index(ctx, index)).u32;
}
void coy_set_uint(coy_context_t* ctx, int32_t index, uint32_t value)
{
    coy_slots_setval_(&ctx->slots, coy_normalize_index(ctx, index), (union coy_register_){.u32=value});
}

bool coy_call(coy_context_t* ctx, const char* module_name, const char* function_name)
{
#if 0
    // ... this is apparently buggy?!
    struct coy_module_entry_* mentry = stbds_shgetp_null(ctx->env->modules, module_name);
#else
    ptrdiff_t mentryidx = stbds_shgeti(ctx->env->modules, module_name);
    struct coy_module_entry_* mentry = mentryidx >= 0 ? &ctx->env->modules[mentryidx] : NULL;
#endif
    if(!mentry) //< error: module not found
        return false;
    COY_ASSERT(mentry->key);
    struct coy_module_* module = mentry->value;
    COY_ASSERT(module);
#if 0
    struct coy_module_symbol_entry_* sentry = stbds_shgetp_null(module->symbols, function_name);
#else
    ptrdiff_t sentryidx = stbds_shgeti(module->symbols, function_name);
    struct coy_module_symbol_entry_* sentry = sentryidx >= 0 ? &module->symbols[sentryidx] : NULL;
#endif
    if(!sentry || sentry->value.category != COY_MODULE_SYMCAT_FUNCTION_)    //< error: symbol not found, or symbol is not a function
        return false;
    struct coy_function_* func;
    if(stbds_arrlenu(sentry->value.u.functions) > 1)
        COY_TODO("overloaded function resolution");
    else
        func = sentry->value.u.functions[0];
    coy_vm_call_(ctx, func, false);
    return true;
}
