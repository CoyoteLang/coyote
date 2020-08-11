#include "compat_shims.h"
#include "../bytecode.h"
#include "stb_ds.h"

#include "../util/bitarray.h"
#include "../util/debug.h"

#include <stdbool.h>
#include <stdarg.h>
#include <limits.h>
#include <inttypes.h>
#include <assert.h>
#include <stdio.h>

#define COY_OP_TRACE_   1

// a continuation is a fiber, generator, or similar --- a "slice" of stack that can be resumed
// TODO: Unused for now, but will replace some uses of coy_stack_segment_ at some point
/*struct coy_continuation_
{
};*/

static void coy_op_handle_add_(coy_context_t* ctx, struct coy_stack_segment_* seg, struct coy_stack_frame_* frame, const union coy_instruction_* instr, uint32_t dstreg)
{
    assert(instr->op.nargs == 2);
    size_t ra = frame->fp + instr[1].arg.index;
    size_t rb = frame->fp + instr[2].arg.index;
    assert(!coy_bitarray_get(&seg->pregs, ra) && "value A in binary op is a reference type");
    assert(!coy_bitarray_get(&seg->pregs, rb) && "value B in binary op is a reference type");
    coy_bitarray_set(&seg->pregs, dstreg, false);
    switch(instr->op.flags & COY_OPFLG_TYPE_MASK)
    {
    case COY_OPFLG_TYPE_INT32:
    case COY_OPFLG_TYPE_UINT32:
        seg->regs[dstreg].u32 = seg->regs[ra].u32 + seg->regs[rb].u32;
        break;
    default:
        assert(0 && "invalid instruction");
    }
}
static void coy_op_handle_sub_(coy_context_t* ctx, struct coy_stack_segment_* seg, struct coy_stack_frame_* frame, const union coy_instruction_* instr, uint32_t dstreg)
{
    assert(instr->op.nargs == 2);
    size_t ra = frame->fp + instr[1].arg.index;
    size_t rb = frame->fp + instr[2].arg.index;
    assert(!coy_bitarray_get(&seg->pregs, ra) && "value A in binary op is a reference type");
    assert(!coy_bitarray_get(&seg->pregs, rb) && "value B in binary op is a reference type");
    coy_bitarray_set(&seg->pregs, dstreg, false);
    switch(instr->op.flags & COY_OPFLG_TYPE_MASK)
    {
    case COY_OPFLG_TYPE_INT32:
    case COY_OPFLG_TYPE_UINT32:
        seg->regs[dstreg].u32 = seg->regs[ra].u32 - seg->regs[rb].u32;
        break;
    default:
        assert(0 && "invalid instruction");
    }
}
static void coy_op_handle_mul_(coy_context_t* ctx, struct coy_stack_segment_* seg, struct coy_stack_frame_* frame, const union coy_instruction_* instr, uint32_t dstreg)
{
    assert(instr->op.nargs == 2);
    size_t ra = frame->fp + instr[1].arg.index;
    size_t rb = frame->fp + instr[2].arg.index;
    assert(!coy_bitarray_get(&seg->pregs, ra) && "value A in binary op is a reference type");
    assert(!coy_bitarray_get(&seg->pregs, rb) && "value B in binary op is a reference type");
    coy_bitarray_set(&seg->pregs, dstreg, false);
    uint32_t a, b, r;
    switch(instr->op.flags & COY_OPFLG_TYPE_MASK)
    {
    case COY_OPFLG_TYPE_INT32:
        // we do unsigned multiplication to avoid C undefined behavior
        a = seg->regs[ra].u32;
        if(a >> 31) a = -a;
        b = seg->regs[rb].u32;
        if(b >> 31) b = -b;
        r = a * b;
        seg->regs[dstreg].u32 = (seg->regs[ra].i32 < 0) == (seg->regs[rb].i32 < 0) ? r : -r;
        break;
    case COY_OPFLG_TYPE_UINT32:
        seg->regs[dstreg].u32 = seg->regs[ra].u32 * seg->regs[rb].u32;
        break;
    default:
        assert(0 && "invalid instruction");
    }
}
static void coy_op_handle_div_(coy_context_t* ctx, struct coy_stack_segment_* seg, struct coy_stack_frame_* frame, const union coy_instruction_* instr, uint32_t dstreg)
{
    assert(instr->op.nargs == 2);
    size_t ra = frame->fp + instr[1].arg.index;
    size_t rb = frame->fp + instr[2].arg.index;
    assert(!coy_bitarray_get(&seg->pregs, ra) && "value A in binary op is a reference type");
    assert(!coy_bitarray_get(&seg->pregs, rb) && "value B in binary op is a reference type");
    coy_bitarray_set(&seg->pregs, dstreg, false);
    uint32_t b;
    switch(instr->op.flags & COY_OPFLG_TYPE_MASK)
    {
    case COY_OPFLG_TYPE_INT32:
        COY_TODO("division of `int`");
    case COY_OPFLG_TYPE_UINT32:
        b = seg->regs[rb].u32;
        if(!b)
            COY_TODO("handling division by 0");
        seg->regs[dstreg].u32 = seg->regs[ra].u32 / b;
        break;
    default:
        assert(0 && "invalid instruction");
    }
}
static void coy_op_handle_rem_(coy_context_t* ctx, struct coy_stack_segment_* seg, struct coy_stack_frame_* frame, const union coy_instruction_* instr, uint32_t dstreg)
{
    assert(instr->op.nargs == 2);
    size_t ra = frame->fp + instr[1].arg.index;
    size_t rb = frame->fp + instr[2].arg.index;
    assert(!coy_bitarray_get(&seg->pregs, ra) && "value A in binary op is a reference type");
    assert(!coy_bitarray_get(&seg->pregs, rb) && "value B in binary op is a reference type");
    coy_bitarray_set(&seg->pregs, dstreg, false);
    uint32_t b;
    switch(instr->op.flags & COY_OPFLG_TYPE_MASK)
    {
    case COY_OPFLG_TYPE_INT32:
        COY_TODO("remainder of `int`");
    case COY_OPFLG_TYPE_UINT32:
        b = seg->regs[rb].u32;
        if(!b)
            COY_TODO("handling remainder by 0");
        seg->regs[dstreg].u32 = seg->regs[ra].u32 % b;
        break;
    default:
        assert(0 && "invalid instruction");
    }
}
static void coy_op_handle_ret_(coy_context_t* ctx, struct coy_stack_segment_* seg, struct coy_stack_frame_* frame, const union coy_instruction_* instr, uint32_t dstreg)
{
    assert(instr->op.nargs <= 1);
    if(instr->op.nargs)
    {
        size_t ra = frame->fp + instr[1].arg.index;
        if(ra != frame->fp) // it's a no-op if this is true
        {
            coy_bitarray_set(&seg->pregs, frame->fp + 0, coy_bitarray_get(&seg->pregs, ra));
            seg->regs[frame->fp + 0] = seg->regs[ra];
        }
    }
    coy_context_pop_frame_(ctx);
}
static void coy_op_handle__dumpu32_(coy_context_t* ctx, struct coy_stack_segment_* seg, struct coy_stack_frame_* frame, const union coy_instruction_* instr, uint32_t dstreg)
{
    // we don't generally want unconditional colors (because of file output), but eh
    printf("\033[35m*** [$%" PRIu32 "]__dumpu32:", dstreg);
    for(size_t i = 1; i <= instr->op.nargs; i++)
    {
        size_t r = frame->fp + instr[i].arg.index;
        printf(" $%" PRIu32 "=%" PRIu32, instr[i].arg.index, seg->regs[r].u32);
    }
    printf("\033[0m\n");
}

typedef void coy_op_handler_(coy_context_t* ctx, struct coy_stack_segment_* seg, struct coy_stack_frame_* frame, const union coy_instruction_* instr, uint32_t dstreg);
static coy_op_handler_* const coy_op_handlers_[256] = {
    [COY_OPCODE_ADD] = coy_op_handle_add_,
    [COY_OPCODE_SUB] = coy_op_handle_sub_,
    [COY_OPCODE_MUL] = coy_op_handle_mul_,
    [COY_OPCODE_DIV] = coy_op_handle_div_,
    [COY_OPCODE_REM] = coy_op_handle_rem_,
    [COY_OPCODE_RET] = coy_op_handle_ret_,
    [COY_OPCODE__DUMPU32] = coy_op_handle__dumpu32_,
};

// runs a frame until it exits
void coy_vm_exec_frame_(coy_context_t* ctx)
{
    struct coy_stack_segment_* seg = ctx->top;
    assert(stbds_arrlenu(seg->frames) && "Cannot execute a frame without a pending frame");
    size_t nframes_start = stbds_arrlenu(seg->frames);
    while(stbds_arrlenu(seg->frames) >= nframes_start)  // we exit when we leave the starting frame
    {
        struct coy_stack_frame_* frame = &seg->frames[stbds_arrlenu(seg->frames) - 1];
        struct coy_function_* func = frame->function;
        // TODO: we'll need to optimize this at some point ...
        size_t slen = frame->bp + func->u.coy.maxregs;
        coy_bitarray_setlen(&seg->pregs, slen);
        stbds_arrsetlen(seg->regs, slen);
        if(!func->blocks)
        {
            int32_t ret = func->u.cfunc(ctx);
            if(ret < 0)
                abort();    // error in cfunc
            coy_context_pop_frame_(ctx);
            continue;
        }
        size_t nframes = stbds_arrlenu(seg->frames);
#if COY_OP_TRACE_
        printf("@function:%p (%" PRIu32 " params)\n", (void*)func, func->blocks[0].nparams);
        uint32_t pblock = UINT32_MAX;
#endif
        do // execute function; this loop is optional, but is done as an optimization
        {
            const union coy_instruction_* instr = &func->u.coy.instrs[frame->pc];
#if COY_OP_TRACE_
            {
                if(pblock != frame->block)
                {
                    pblock = frame->block;
                    printf(".block%" PRIu32 " (", pblock);
                    for(uint32_t i = 0; i < func->blocks[frame->block].nparams; i++)
                    {
                        if(i) putchar(' ');
                        printf("$%" PRIu32 "=%" PRIu32, i, seg->regs[frame->fp+i].u32);
                    }
                    printf(")\n");
                }
                const char* name = coy_instruction_opcode_names_[instr->op.code];
                if(!name) name = "<?>";
                printf("\t$%u = %s", (unsigned)((frame->bp - frame->fp) + frame->pc), name);
                for(size_t i = 1; i <= instr->op.nargs; i++)
                {
                    printf(" $%" PRIu32, instr[i].arg.index);
                }
                printf("\n");
            }
#endif
            uint32_t dstreg = frame->bp + frame->pc - func->blocks[frame->block].offset;
            frame->pc += 1U + instr->op.nargs;
            coy_op_handler_* h = coy_op_handlers_[instr->op.code];
            assert(h && "Invalid instruction"); // for now, we trust the bytecode
            h(ctx, seg, frame, instr, dstreg);
#if COY_OP_TRACE_
            printf("\t\tR:=%" PRIu32 "\n", seg->regs[dstreg].u32);
#endif
        }
        while(stbds_arrlenu(seg->frames) == nframes);
    }
}

void coy_push_uint(coy_context_t* ctx, uint32_t u)
{
    struct coy_stack_segment_* seg = ctx->top;
    coy_bitarray_push(&seg->pregs, false);
    stbds_arraddnptr(seg->regs, 1)->u32 = u;
}

void vm_test_basicDBG(void)
{
    static const struct coy_function_block_ in_blocks[] = {
        {2,0},
    };
    static const union coy_instruction_ in_instrs[] = {
        /*2*/   {.op={COY_OPCODE_ADD, COY_OPFLG_TYPE_UINT32, 0, 2}},
        /*3*/   {.arg={0,0}},
        /*4*/   {.arg={1,0}},
        /*-*/   {.op={COY_OPCODE__DUMPU32, 0, 0, 3}},
        /*-*/   {.arg={0,0}},
        /*-*/   {.arg={1,0}},
        /*-*/   {.arg={2,0}},
        /*5*/   {.op={COY_OPCODE_RET, 0, 0, 1}},
        /*6*/   {.arg={2,0}},
    };

    struct coy_function_ func = {NULL};
    stbds_arrsetlen(func.blocks, sizeof(in_blocks) / sizeof(*in_blocks));
    memcpy(func.blocks, in_blocks, sizeof(in_blocks));
    stbds_arrsetlen(func.u.coy.instrs, sizeof(in_instrs) / sizeof(*in_instrs));
    memcpy(func.u.coy.instrs, in_instrs, sizeof(in_instrs));
    coy_function_update_maxregs_(&func);

    coy_env_t env;
    coy_env_init(&env);

    coy_context_t* ctx = coy_context_create(&env);
    coy_context_push_frame_(ctx, &func, 2, false);
    coy_push_uint(ctx, 5);
    coy_push_uint(ctx, 7);
    coy_vm_exec_frame_(ctx);

    printf("RESULT: %" PRIu32 "\n", ctx->top->regs[0].u32);

    //coy_env_deinit(&env);   //< not yet implemented
}

/*
Public API inspiration:
    https://wren.io/embedding/slots-and-handles.html
*/
