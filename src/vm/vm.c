#include "vm.h"
#include "stb_ds.h"

#include "../util/bitarray.h"

#include <stdbool.h>
#include <stdarg.h>
#include <limits.h>
#include <inttypes.h>
#include <assert.h>
#include <stdio.h>

#define COY_OP_TRACE_   1

// this can grow, so we can be a bit conservative
#define COY_THREAD_INITIAL_STACK_REG_SIZE_      2048
#define COY_THREAD_INITIAL_STACK_FRAME_SIZE_    16

// this will eventually be in its own file
bool coy_ensure_(bool v, const char* file, int line, const char* format, ...)
{
    if(!v)
    {
        va_list args;
        va_start(args, format);
        fputs("COY_ENSURE failed: ", stderr);
        vfprintf(stderr, format, args);
        putc('\n', stderr);
        va_end(args);
        assert(false);
    }
    return v;
}
#define COY_ENSURE(x, ...)  coy_ensure_(!!(x), __FILE__, __LINE__, __VA_ARGS__)

typedef void coy_gc_mark_function_(struct coy_gc_* gc, void* ptr);
typedef void coy_gc_dtor_function_(struct coy_gc_* gc, void* ptr);
enum coy_typeinfo_category_
{
    COY_TYPEINFO_CAT_INTERNAL_,  //< should never, ever show up in user code
    COY_TYPEINFO_CAT_NORETURN_,
    COY_TYPEINFO_CAT_INTEGER_,
};
struct coy_typeinfo_
{
    enum coy_typeinfo_category_ category;
    union
    {
        const char* internal_name;  // for debugging
        struct { uint8_t is_signed; uint8_t width; } integer;
    } u;
    coy_gc_mark_function_* cb_mark;
    coy_gc_dtor_function_* cb_dtor;
};

struct coy_gcobj_
{
    const struct coy_typeinfo_* typeinfo;           // type information
    size_t index : sizeof(size_t) * CHAR_BIT - 2;   //< index in set
    size_t set : 2;                                 //< active set (2==thread root, 3==common root)
};
static struct coy_gc_* coy_gc_init_(struct coy_gc_* gc)
{
    if(!gc) return NULL;
    for(size_t i = 0; i < sizeof(gc->sets) / sizeof(*gc->sets); i++)
        gc->sets[i] = NULL;
    gc->curset = 0;
    return gc;
}
static void coy_gc_mark_(struct coy_gc_* gc, void* ptr)
{
    if(!ptr) return;    // nothing to mark
    struct coy_gcobj_* gcobj = (struct coy_gcobj_*)((char*)ptr - sizeof(struct coy_gcobj_));
    if(gcobj->set >= 2) return; // it's a root or common (means: always marked)
    if(gcobj->set == gc->curset) return;    // already marked
    gcobj->set = gc->curset;

    // we do a move from uset ("unmarked set") to mset ("marked set")
    struct coy_gcobj_** mset = gc->sets[gc->curset];
    struct coy_gcobj_** uset = gc->sets[!gc->curset];
    // remove from `uset` by assigning last item to current (& shortening array by 1)
    stbds_arrdelswap(uset, gcobj->index);
    if(stbds_arrlenu(uset)) uset[gcobj->index]->index = gcobj->index;
    // insert into `mset` by appending at end
    gcobj->index = stbds_arrlenu(mset);
    stbds_arrput(mset, gcobj);
    // stbds may have changed the ptrs
    gc->sets[gc->curset] = mset;
    gc->sets[!gc->curset] = uset;

    // recurse, to mark children of this object
    if(gcobj->typeinfo->cb_mark) gcobj->typeinfo->cb_mark(gc, ptr);
}
void* coy_gc_malloc_(struct coy_gc_* gc, size_t size, const struct coy_typeinfo_* typeinfo)
{
    char* gcobj = malloc(sizeof(struct coy_gcobj_) + size);
    if(!COY_ENSURE(gcobj, "failed to allocate %" PRIu64 " bytes", (uint64_t)size))
        return NULL;
    struct coy_gcobj_** putset = gc->sets[!gc->curset];
    struct coy_gcobj_ g = {
        .typeinfo = typeinfo,
        .index = stbds_arrlenu(putset),
        .set = !gc->curset,  //< the object begins its life as unmarked
    };
    memcpy(gcobj, &g, sizeof(g));
    stbds_arrput(putset, (struct coy_gcobj_*)gcobj);
    gc->sets[!gc->curset] = putset;
    return gcobj + sizeof(g);
}

// A register is a basic unit of storage.
union coy_register_
{
    int32_t i32;
    uint32_t u32;
    int64_t i64;
    uint64_t u64;
    double d;
    void* ptr;
};

// A frame is a single function context, storing any information necessary for its execution.
struct coy_frame_
{
    // "frame pointer"; points to location on stack of register $0
    // all reads are offset by `fp`
    uint32_t fp;
    // "base pointer", or `fp+nparams`; points to location on stack of first instruction's result
    // all writes are offset by `bp`
    uint32_t bp;
    // "block index"; which block are we in?
    uint32_t block;
    // "program counter"
    uint32_t pc;
    struct coy_function_* function;
};

// A stack segment is a contiguous segment of registers.
struct coy_stack_segment_
{
    struct coy_stack_segment_* parent;
    coy_bitarray_t pregs;   //< which registers are pointers
    union coy_register_* regs;
    // TODO: frames could eventually be merged into `regs`, with a clever use of the stack
    // (but this is easier for debugging)
    struct coy_frame_* frames;
};

// a continuation is a fiber, generator, or similar --- a "slice" of stack that can be resumed
// TODO: Unused for now, but will replace some uses of coy_stack_segment_ at some point
/*struct coy_continuation_
{
};*/

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
static struct coy_stack_segment_* coy_thread_create_stack_segment_(coy_thread_t* thread)
{
    struct coy_stack_segment_* seg = coy_gc_malloc_(&thread->gc, sizeof(struct coy_stack_segment_), &coy_ti_stack_segment_);
    seg->parent = thread->top;
    coy_bitarray_init(&seg->pregs);
    seg->regs = NULL;
    stbds_arrsetcap(seg->regs, COY_THREAD_INITIAL_STACK_REG_SIZE_);
    seg->frames = NULL;
    stbds_arrsetcap(seg->frames, COY_THREAD_INITIAL_STACK_FRAME_SIZE_);
    return seg;
}

coy_vm_t* coy_vm_init(coy_vm_t* vm)
{
    if(!vm) return NULL;
    vm->threads.ptr = NULL;
    vm->threads.next_id = 1;
    vm->modules = NULL;
    return vm;
}

coy_thread_t* coy_vm_create_thread(coy_vm_t* vm)
{
    coy_thread_t* thread = malloc(sizeof(coy_thread_t));
    thread->vm = vm;
    coy_gc_init_(&thread->gc);
    thread->index = stbds_arrlenu(vm->threads.ptr);
    thread->id = vm->threads.next_id++;
    thread->top = NULL;
    thread->top = coy_thread_create_stack_segment_(thread);
    return stbds_arrput(vm->threads.ptr, thread);
}
coy_thread_t* coy_vm_get_thread(coy_vm_t* vm, size_t i)
{
    if(!COY_ENSURE(vm && i < stbds_arrlenu(vm->threads.ptr), "VM is null or thread index is out of bounds"))
        return NULL;
    return vm->threads.ptr[i];
}

struct coy_method_overload_
{
    struct coy_function_* function;
    char signature[];
};
struct coy_method_entry_
{
    char* key;
    struct coy_method_overload_** value;
};
struct coy_variable_entry_
{
    char* key;
    uint32_t value;
};

// Contains a call table for a refval.
struct coy_refval_vtbl_
{
    struct coy_method_entry_* static_methods;   // hash table
    struct coy_method_entry_* methods;          // hash table
    uint32_t vars_nref; // number of reference variables
    uint32_t vars_narr; // number of array variables
    struct coy_typeinfo_** vartypes;
    struct coy_variable_entry_* variables;      // hash table
};

#define COY_ENUM_instruction_opcode_(ITEM,VAL,VLAST)    \
    ITEM(ADD),              \
    ITEM(RET),              \
    ITEM(_DUMPU32)VAL(0xF0) \
    VLAST(0xFF)

enum coy_instruction_opcode_
{
#define COY_ITEM_(NAME)    COY_OPCODE_##NAME
#define COY_VAL_(V)        = (V)
#define COY_VLAST_(V)      ,COY_OPCODE_LAST_ = (V)
    COY_ENUM_instruction_opcode_(COY_ITEM_,COY_VAL_,COY_VLAST_)
#undef COY_ITEM_
#undef COY_VAL_
#undef COY_VLAST_
};

static const char* const coy_instruction_opcode_names_[256] = {
#define COY_ITEM_(NAME)    [COY_OPCODE_##NAME] = #NAME
#define COY_VAL_(V)
#define COY_VLAST_(V)
    COY_ENUM_instruction_opcode_(COY_ITEM_,COY_VAL_,COY_VLAST_)
};

// type
#define COY_OPFLG_SIGNED    0x00
#define COY_OPFLG_FLOAT     0x40
#define COY_OPFLG_UNSIGNED  0x80
#define COY_OPFLG_POINTER   0xC0
// width (non-ptr)
#define COY_OPFLG_8BIT      0x00
#define COY_OPFLG_16BIT     0x10
#define COY_OPFLG_32BIT     0x20
#define COY_OPFLG_64BIT     0x30
// subtype (ptr)
#define COY_OPFLG_REFVAL    0x00    // refval
#define COY_OPFLG_ARR       0x10    // array
#define COY_OPFLG_BOXED     0x20    // boxed struct
#define COY_OPFLG_RESERVED1 0x30    // reserved

#define COY_OPFLG_TYPE_INT32    (COY_OPFLG_SIGNED | COY_OPFLG_32BIT)
#define COY_OPFLG_TYPE_UINT32   (COY_OPFLG_UNSIGNED | COY_OPFLG_32BIT)

#define COY_OPFLG_TYPE_MASK     (0xC0 | 0x30)

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

void coy_thread_create_frame_(coy_thread_t* thread, struct coy_function_* function, uint32_t nparams, bool segmented)
{
    if(!thread->top || segmented)
        thread->top = coy_thread_create_stack_segment_(thread);
    struct coy_stack_segment_* seg = thread->top;

    struct coy_frame_ frame;
    frame.fp = stbds_arrlenu(seg->regs);
    frame.bp = frame.fp + nparams;
    frame.block = 0;
    frame.pc = 0;
    frame.function = function;
    stbds_arrput(seg->frames, frame);
}
void coy_thread_pop_frame_(coy_thread_t* thread)
{
    assert(thread->top);
    struct coy_stack_segment_* seg = thread->top;
    size_t nframes = stbds_arrlenu(seg->frames);
    assert(nframes);
    stbds_arrsetlen(seg->frames, nframes - 1);
    if(nframes == 1 && seg->parent)    // if we had 1 frame earlier, then the entire stack segment can be destroyed
        thread->top = seg->parent;
}

static void coy_op_handle_add_(coy_thread_t* thread, struct coy_stack_segment_* seg, struct coy_frame_* frame, const union coy_instruction_* instr, uint32_t dstreg)
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
static void coy_op_handle_ret_(coy_thread_t* thread, struct coy_stack_segment_* seg, struct coy_frame_* frame, const union coy_instruction_* instr, uint32_t dstreg)
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
    coy_thread_pop_frame_(thread);
}
static void coy_op_handle__dumpu32_(coy_thread_t* thread, struct coy_stack_segment_* seg, struct coy_frame_* frame, const union coy_instruction_* instr, uint32_t dstreg)
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

typedef void coy_op_handler_(coy_thread_t* thread, struct coy_stack_segment_* seg, struct coy_frame_* frame, const union coy_instruction_* instr, uint32_t dstreg);
static coy_op_handler_* const coy_op_handlers_[256] = {
    [COY_OPCODE_ADD] = coy_op_handle_add_,
    [COY_OPCODE_RET] = coy_op_handle_ret_,
    [COY_OPCODE__DUMPU32] = coy_op_handle__dumpu32_,
};

// runs a frame until it exits
void coy_thread_exec_frame_(coy_thread_t* thread)
{
    struct coy_stack_segment_* seg = thread->top;
    assert(stbds_arrlenu(seg->frames) && "Cannot execute a frame without a pending frame");
    size_t nframes_start = stbds_arrlenu(seg->frames);
    while(stbds_arrlenu(seg->frames) >= nframes_start)  // we exit when we leave the starting frame
    {
        struct coy_frame_* frame = &seg->frames[stbds_arrlenu(seg->frames) - 1];
        struct coy_function_* func = frame->function;
        // TODO: we'll need to optimize this at some point ...
        size_t slen = frame->bp + func->u.coy.maxregs;
        coy_bitarray_setlen(&seg->pregs, slen);
        stbds_arrsetlen(seg->regs, slen);
        if(!func->blocks)
        {
            int32_t ret = func->u.cfunc(thread);
            if(ret < 0)
                abort();    // error in cfunc
            coy_thread_pop_frame_(thread);
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
            h(thread, seg, frame, instr, dstreg);
#if COY_OP_TRACE_
            printf("\t\tR:=%" PRIu32 "\n", seg->regs[dstreg].u32);
#endif
        }
        while(stbds_arrlenu(seg->frames) == nframes);
    }
}

void coy_push_uint(coy_thread_t* thread, uint32_t u)
{
    struct coy_stack_segment_* seg = thread->top;
    coy_bitarray_push(&seg->pregs, false);
    stbds_arraddnptr(seg->regs, 1)->u32 = u;
}

void coy_function_update_maxregs_(struct coy_function_* func)
{
    if(!func->blocks) return;   // it's a native function, so we have nothing to do
    uint32_t maxregs = 0;
    size_t nblocks = stbds_arrlenu(func->blocks);
    for(size_t b = 0; b < nblocks; b++)
    {
        struct coy_function_block_* block = &func->blocks[b];
        uint32_t blen = (b + 1 < nblocks ? func->blocks[b+1].offset : stbds_arrlenu(func->u.coy.instrs)) - block->offset;
        assert(blen);   // empty blocks should not exist
        blen += block->nparams; // effective length is increased by this
        if(blen > maxregs)
            maxregs = blen;
    }
    func->u.coy.maxregs = maxregs;
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

    coy_vm_t vm;
    coy_vm_init(&vm);

    coy_thread_t* thread = coy_vm_create_thread(&vm);
    coy_thread_create_frame_(thread, &func, 2, false);
    coy_push_uint(thread, 5);
    coy_push_uint(thread, 7);
    coy_thread_exec_frame_(thread);

    printf("RESULT: %" PRIu32 "\n", thread->top->regs[0].u32);

    //coy_vm_deinit(&vm);   //< not yet implemented
}
