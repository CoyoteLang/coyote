#include "function.h"
#include "context.h"
#include "../bytecode.h"
#include "stb_ds.h"

#include "../util/bitarray.h"
#include "../util/debug.h"

#include <string.h>
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

static union coy_register_ coy_op_getreg_(struct coy_stack_segment_* seg, struct coy_stack_frame_* frame, union coy_instruction_ regarg, bool* isptr)
{
    if(regarg.arg.isconst)
    {
        if(isptr) *isptr = regarg.arg.index < frame->function->u.coy.consts.nsymbols + frame->function->u.coy.consts.nrefs;
        return frame->function->u.coy.consts.data[regarg.arg.index];
    }
    else
        return coy_slots_get_(&seg->slots, frame->fp + regarg.arg.index, isptr);
}
static void coy_op_copyreg_(struct coy_slots_* dst, size_t d, struct coy_stack_segment_* seg, struct coy_stack_frame_* frame, union coy_instruction_ regarg)
{
    bool isptr;
    union coy_register_ reg = coy_op_getreg_(seg, frame, regarg, &isptr);
    coy_slots_set_(dst, d, reg, isptr);
}

static bool coy_op_handle_add_(coy_context_t* ctx, struct coy_stack_segment_* seg, struct coy_stack_frame_* frame, const union coy_instruction_* instr, uint32_t dstreg)
{
    COY_CHECK(instr->op.nargs == 2);
    union coy_register_ a = coy_op_getreg_(seg, frame, instr[1], NULL);
    union coy_register_ b = coy_op_getreg_(seg, frame, instr[2], NULL);
    union coy_register_ dst;
    switch(instr->op.flags & COY_OPFLG_TYPE_MASK)
    {
    case COY_OPFLG_TYPE_INT32:
    case COY_OPFLG_TYPE_UINT32:
        dst.u32 = a.u32 + b.u32;
        break;
    default:
        COY_CHECK_MSG(false, "invalid instruction");
    }
    coy_slots_setval_(&seg->slots, dstreg, dst);
    return true;
}
static bool coy_op_handle_sub_(coy_context_t* ctx, struct coy_stack_segment_* seg, struct coy_stack_frame_* frame, const union coy_instruction_* instr, uint32_t dstreg)
{
    COY_CHECK(instr->op.nargs == 2);
    union coy_register_ a = coy_op_getreg_(seg, frame, instr[1], NULL);
    union coy_register_ b = coy_op_getreg_(seg, frame, instr[2], NULL);
    union coy_register_ dst;
    switch(instr->op.flags & COY_OPFLG_TYPE_MASK)
    {
    case COY_OPFLG_TYPE_INT32:
    case COY_OPFLG_TYPE_UINT32:
        dst.u32 = a.u32 - b.u32;
        break;
    default:
        COY_CHECK_MSG(false, "invalid instruction");
    }
    coy_slots_setval_(&seg->slots, dstreg, dst);
    return true;
}
static bool coy_op_handle_mul_(coy_context_t* ctx, struct coy_stack_segment_* seg, struct coy_stack_frame_* frame, const union coy_instruction_* instr, uint32_t dstreg)
{
    COY_CHECK(instr->op.nargs == 2);
    union coy_register_ a = coy_op_getreg_(seg, frame, instr[1], NULL);
    union coy_register_ b = coy_op_getreg_(seg, frame, instr[2], NULL);
    union coy_register_ dst;
    bool negresult;
    switch(instr->op.flags & COY_OPFLG_TYPE_MASK)
    {
    case COY_OPFLG_TYPE_INT32:
        // we do unsigned multiplication to avoid C undefined behavior
        negresult = (a.i32 < 0) != (b.i32 < 0);
        a.u32 = a.i32;  //< to ensure proper conversion
        if(a.u32 >> 31) a.u32 = -a.u32;
        b.u32 = b.i32;  //< to ensure proper conversion
        if(b.u32 >> 31) b.u32 = -b.u32;
        dst.u32 = a.u32 * b.u32;
        if(negresult)
            dst.u32 = -dst.u32;
        break;
    case COY_OPFLG_TYPE_UINT32:
        dst.u32 = a.u32 * b.u32;
        break;
    default:
        COY_CHECK_MSG(false, "invalid instruction");
    }
    coy_slots_setval_(&seg->slots, dstreg, dst);
    return true;
}
static bool coy_op_handle_div_(coy_context_t* ctx, struct coy_stack_segment_* seg, struct coy_stack_frame_* frame, const union coy_instruction_* instr, uint32_t dstreg)
{
    COY_CHECK(instr->op.nargs == 2);
    union coy_register_ a = coy_op_getreg_(seg, frame, instr[1], NULL);
    union coy_register_ b = coy_op_getreg_(seg, frame, instr[2], NULL);
    union coy_register_ dst;
    switch(instr->op.flags & COY_OPFLG_TYPE_MASK)
    {
    case COY_OPFLG_TYPE_INT32:
        COY_TODO("division of `int`");
    case COY_OPFLG_TYPE_UINT32:
        if(!b.u32)
            COY_TODO("handling division by 0");
        dst.u32 = a.u32 / b.u32;
        break;
    default:
        COY_CHECK_MSG(false, "invalid instruction");
    }
    coy_slots_setval_(&seg->slots, dstreg, dst);
    return true;
}
static bool coy_op_handle_rem_(coy_context_t* ctx, struct coy_stack_segment_* seg, struct coy_stack_frame_* frame, const union coy_instruction_* instr, uint32_t dstreg)
{
    COY_CHECK(instr->op.nargs == 2);
    union coy_register_ a = coy_op_getreg_(seg, frame, instr[1], NULL);
    union coy_register_ b = coy_op_getreg_(seg, frame, instr[2], NULL);
    union coy_register_ dst;
    switch(instr->op.flags & COY_OPFLG_TYPE_MASK)
    {
    case COY_OPFLG_TYPE_INT32:
        COY_TODO("remainder of `int`");
    case COY_OPFLG_TYPE_UINT32:
        if(!b.u32)
            COY_TODO("handling remainder by 0");
        dst.u32 = a.u32 % b.u32;
        break;
    default:
        COY_CHECK_MSG(false, "invalid instruction");
    }
    coy_slots_setval_(&seg->slots, dstreg, dst);
    return true;
}
static void coy_op_handle_jmp_helper_(coy_context_t* ctx, struct coy_stack_segment_* seg, struct coy_stack_frame_* frame, const union coy_instruction_* instr, uint32_t block_arg, uint32_t mov_head, uint32_t mov_tail)
{
    COY_ASSERT(mov_head <= mov_tail);
    uint32_t block = instr[1+block_arg].raw;
    uint32_t mov_num = mov_tail - mov_head;
    // move temp <= stack
    for(size_t i = 0; i < mov_num; i++)
        coy_op_copyreg_(&ctx->slots, i, seg, frame, instr[1+mov_head+i]);
    // move stack <= temp
    for(size_t i = 0; i < mov_num; i++)
        coy_slots_copy_(&seg->slots, frame->fp + i, &ctx->slots, i);
    COY_CHECK(block < stbds_arrlenu(frame->function->u.coy.blocks));
    frame->block = block;
    struct coy_function_block_* blockinfo = &frame->function->u.coy.blocks[block];
    frame->bp = frame->fp + blockinfo->nparams;
    frame->pc = blockinfo->offset;
}
static bool coy_op_handle_jmp_(coy_context_t* ctx, struct coy_stack_segment_* seg, struct coy_stack_frame_* frame, const union coy_instruction_* instr, uint32_t dstreg)
{
    coy_op_handle_jmp_helper_(ctx, seg, frame, instr, 0, 1, instr->op.nargs);
    return true;
}
static bool coy_op_handle_jmpc_(coy_context_t* ctx, struct coy_stack_segment_* seg, struct coy_stack_frame_* frame, const union coy_instruction_* instr, uint32_t dstreg)
{
    COY_CHECK(instr->op.nargs >= 5);
    union coy_register_ a = coy_op_getreg_(seg, frame, instr[1], NULL);
    union coy_register_ b = coy_op_getreg_(seg, frame, instr[2], NULL);
    bool testeq, testlt;
    switch(instr->op.flags & COY_OPFLG_TYPE_MASK)
    {
    case COY_OPFLG_TYPE_INT32:
        testeq = a.i32 == b.i32;
        testlt = a.i32 < b.i32;
        break;
    case COY_OPFLG_TYPE_UINT32:
        testeq = a.u32 == b.u32;
        testlt = a.u32 < b.u32;
        break;
    default:
        testeq = testlt = false;
        COY_CHECK_MSG(false, "invalid instruction");
    }
    bool test;
    switch(instr->op.flags & COY_OPFLG_CMP_MASK)
    {
    case COY_OPFLG_CMP_EQ: test = testeq; break;
    case COY_OPFLG_CMP_NE: test = !testeq; break;
    case COY_OPFLG_CMP_LE: test = testlt || testeq; break;
    case COY_OPFLG_CMP_LT: test = testlt; break;
    default:
        test = false;
        COY_UNREACHABLE();
    }
    uint32_t moves_sep = instr[5].raw;
    if(test)
        coy_op_handle_jmp_helper_(ctx, seg, frame, instr, 2, 5, 5 + moves_sep);
    else
        coy_op_handle_jmp_helper_(ctx, seg, frame, instr, 3, 5 + moves_sep, instr->op.nargs);
    return true;
}
static bool coy_op_handle_call_(coy_context_t* ctx, struct coy_stack_segment_* seg, struct coy_stack_frame_* frame, const union coy_instruction_* instr, uint32_t dstreg)
{
    // TODO: verify type
    struct coy_function_* func = coy_op_getreg_(seg, frame, instr[1], NULL).ptr;
    if(func->attrib & COY_FUNCTION_ATTRIB_NATIVE_)
    {
        COY_TODO("coy->native calls");
    }
    else
    {
        uint32_t frameidx = frame - seg->frames;
        frame->pc -= 1u + instr->op.nargs;  // workaround: push_frame_ relies on pc being at the *start* of a frame (TODO: remove this)
        coy_context_push_frame_(ctx, func, false, false);
        // pushing a frame could invalidate the pointer, so we fix this up
        frame = &seg->frames[frameidx];
        frame->pc += 1u + instr->op.nargs;  // workaround: undo change done for push_frame_ (TODO: remove this)
        struct coy_stack_frame_* nframe = &seg->frames[stbds_arrlenu(seg->frames) - 1u];
        for(uint32_t a = 0; a < instr->op.nargs; a++)
            coy_op_copyreg_(&seg->slots, nframe->fp + a, seg, frame, instr[2+a]);
    }
    //coy_op_handle_jmp_helper_(ctx, seg, frame, instr, 0, 1, instr->op.nargs);
    return false;
}
static bool coy_op_handle_retcall_(coy_context_t* ctx, struct coy_stack_segment_* seg, struct coy_stack_frame_* frame, const union coy_instruction_* instr, uint32_t dstreg)
{
    bool isptr;
    struct coy_function_* nfunction = coy_op_getreg_(seg, frame, instr[1], &isptr).ptr;
    COY_ASSERT(isptr);
    struct coy_stack_frame_ nframe = {0};
    uint32_t nargs = instr->op.nargs - 1u;
    COY_CHECK(nargs == nfunction->u.coy.blocks[0].nparams);
    nframe.fp = frame->fp;
    nframe.bp = frame->fp + nargs;
    nframe.block = 0;
    nframe.return_native = frame->return_native;
    nframe.pc = 0;
    nframe.function = nfunction;
    // move temp <= stack
    for(size_t i = 0; i < nargs; i++)
        coy_op_copyreg_(&ctx->slots, i, seg, frame, instr[2+i]);
    coy_slots_setlen_(&seg->slots, nframe.fp + nfunction->u.coy.maxslots);
    // move stack <= temp
    for(size_t i = 0; i < nargs; i++)
        coy_slots_copy_(&seg->slots, nframe.fp + i, &ctx->slots, i);
    *frame = nframe;
    return false;
}
static bool coy_op_handle_ret_(coy_context_t* ctx, struct coy_stack_segment_* seg, struct coy_stack_frame_* frame, const union coy_instruction_* instr, uint32_t dstreg)
{
    COY_CHECK(instr->op.nargs <= 1);
    if(frame->return_native)
    {
        coy_slots_setlen_(&ctx->slots, instr->op.nargs);
        if(instr->op.nargs)
            coy_op_copyreg_(&ctx->slots, 0, seg, frame, instr[1]);
    }
    else if(instr->op.nargs)
        coy_op_copyreg_(&seg->slots, frame->fp + 0, seg, frame, instr[1]);
    coy_context_pop_frame_(ctx);
    return false;
}
static bool coy_op_handle__dumpu32_(coy_context_t* ctx, struct coy_stack_segment_* seg, struct coy_stack_frame_* frame, const union coy_instruction_* instr, uint32_t dstreg)
{
    // we don't generally want unconditional colors (because of file output), but eh
    printf("\033[35m*** [$%" PRIu32 "]__dumpu32:", dstreg);
    for(size_t i = 1; i <= instr->op.nargs; i++)
    {
        size_t r = frame->fp + instr[i].arg.index;
        union coy_register_ reg = coy_slots_getval_(&seg->slots, r);
        printf(" $%" PRIu32 "=%" PRIu32, instr[i].arg.index, reg.u32);
    }
    printf("\033[0m\n");
    return true;
}

typedef bool coy_op_handler_(coy_context_t* ctx, struct coy_stack_segment_* seg, struct coy_stack_frame_* frame, const union coy_instruction_* instr, uint32_t dstreg);
static coy_op_handler_* const coy_op_handlers_[256] = {
    [COY_OPCODE_ADD]    = coy_op_handle_add_,       // add $a, $b
    [COY_OPCODE_SUB]    = coy_op_handle_sub_,       // sub $a, $b
    [COY_OPCODE_MUL]    = coy_op_handle_mul_,       // mul $a, $b
    [COY_OPCODE_DIV]    = coy_op_handle_div_,       // div $a, $b
    [COY_OPCODE_REM]    = coy_op_handle_rem_,       // rem $a, $b
    [COY_OPCODE_JMP]    = coy_op_handle_jmp_,       // jmp <block>, $vmaps...
    [COY_OPCODE_JMPC]   = coy_op_handle_jmpc_,      // jmpc $a, $b, <block1>, <block2>, <nargs1>, $vmaps...
    [COY_OPCODE_CALL]   = coy_op_handle_call_,      // call $func $args...
    [COY_OPCODE_RETCALL]= coy_op_handle_retcall_,   // retcall $func $args...
    [COY_OPCODE_RET]    = coy_op_handle_ret_,       // ret $vals...
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
        size_t slen = frame->bp + func->u.coy.maxslots;
        coy_slots_setlen_(&seg->slots, slen);
        if(func->attrib & COY_FUNCTION_ATTRIB_NATIVE_)
        {
            int32_t ret = func->u.nat.handler(ctx, func->u.nat.udata);
            if(ret < 0)
                abort();    // error in cfunc
            coy_context_pop_frame_(ctx);
            continue;
        }
#if COY_OP_TRACE_
        printf("@function:%p (%" PRIu32 " params)\n", (void*)func, func->u.coy.blocks[0].nparams);
        uint32_t pblock = UINT32_MAX;
#endif
        bool sameframe;
        do // execute function; this loop is optional, but is done as an optimization
        {
            const union coy_instruction_* instr = &func->u.coy.instrs[frame->pc];
            uint32_t dstreg = frame->bp + frame->pc - func->u.coy.blocks[frame->block].offset;
#if COY_OP_TRACE_
            {
                if(pblock != frame->block)
                {
                    pblock = frame->block;
                    printf(".block%" PRIu32 " (", pblock);
                    for(uint32_t i = 0; i < func->u.coy.blocks[frame->block].nparams; i++)
                    {
                        if(i) putchar(' ');
                        bool isptr;
                        union coy_register_ reg = coy_slots_get_(&seg->slots, frame->fp+i, &isptr);
                        if(isptr)
                            printf("$%" PRIu32 "=%p", i, reg.ptr);
                        else
                            printf("$%" PRIu32 "=%" PRIu32, i, reg.u32);
                    }
                    printf(")\n");
                }
                const char* name = coy_instruction_opcode_names_[instr->op.code];
                if(!name) name = "<?>";
                printf("\t$%" PRIu32 " = %s", dstreg - frame->fp, name);
                for(size_t i = 1; i <= instr->op.nargs; i++)
                {
                    printf(" %s$%" PRIu32, instr[i].arg.isconst ? "c" : "", instr[i].arg.index);
                }
                printf("\n");
            }
#endif
            frame->pc += 1U + instr->op.nargs;
            coy_op_handler_* h = coy_op_handlers_[instr->op.code];
            assert(h && "Invalid instruction"); // for now, we trust the bytecode
            sameframe = h(ctx, seg, frame, instr, dstreg);
#if COY_OP_TRACE_
            if(sameframe)
            {
                bool isptr;
                union coy_register_ reg = coy_slots_get_(&seg->slots, dstreg, &isptr);
                if(isptr)
                    printf("\t\tR:=%p\n", reg.ptr);
                else
                    printf("\t\tR:=%" PRIu32 "\n", reg.u32);
            }
#endif
        }
        while(sameframe);
    }
    // TODO: determine number of return values
    //coy_slots_setlen_(&ctx->slots, 1);
    //bool isptr;
    //union coy_register_ ret = coy_slots_get_(&ctx->top->slots, 0, &isptr);
    //coy_slots_set_(&ctx->slots, 0, ret, isptr);
}

bool coy_vm_call_(struct coy_context* ctx, struct coy_function_* function, bool segmented)
{
    if(!COY_ENSURE(function->u.coy.blocks[0].nparams <= coy_slots_getlen_(&ctx->slots), "misuse: invalid number of parameters passed to the function"))
        return false;
    coy_context_push_frame_(ctx, function, segmented, true);
    struct coy_stack_frame_* frame = coy_context_get_top_frame_(ctx);
    COY_ASSERT(frame);
    memcpy(ctx->top->slots.regs + frame->fp, ctx->slots.regs, function->u.coy.blocks[0].nparams * sizeof(union coy_register_));
    if(!(function->attrib & COY_FUNCTION_ATTRIB_NATIVE_))
        coy_slots_setlen_(&ctx->slots, function->u.coy.maxslots);   //< TODO: set # of slots to maxparams (a lower number) to save memory
    coy_vm_exec_frame_(ctx);
    return true;
}

/*
Public API inspiration:
    https://wren.io/embedding/slots-and-handles.html
*/
