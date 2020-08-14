#include "function2.h"
#include "../util/debug.h"
#include "register.h"
#include "../bytecode.h"

#include "stb_ds.h"
#include <string.h>

// temporary (for COY_VERIFY_, will be moved elsewhere eventually)
#include <stdio.h>

/*
// Constants are always stored in the following order (to simplify the linker & GC):
// - symbols
// - reference constants
// - other constants
// Order *within* a group of constants is arbitrary.
struct coy_function2_constants_
{
    uint32_t nsymbols;  //< how many constants are symbols?
    uint32_t nrefs;     //< how many constants are references?
    // (remaining constants are simple values)
    union coy_register_* values;    //< values themselves
};

// this will eventually replace coy_function_
struct coy_function2_block_
{
    uint32_t nparams;   //< number of parameters
    uint32_t offset;    //< offset to start of block (entry block must start at 0!)
    uint32_t* ptrs;     //< which registers are pointers?
};
struct coy_function2_
{
    const struct coy_typeinfo_* type;   //< this is a function type, *not* return type
    union
    {
        struct
        {
            struct coy_function2_constants_ consts;
            struct coy_function2_block_* blocks;
            union coy_instruction_* instrs;
            uint32_t maxregs;
        } coy;
        struct
        {
            coy_c_function_t* handler;
            void* udata;
        } nat;
    } u;
    uint32_t attrib;
};
*/

struct coy_memio_
{
    const uint8_t* data;
    size_t pos;
    size_t len;
    bool ok;
};
size_t coy_memio_read_at_(struct coy_memio_* memio, size_t pos, void* buf, size_t len)
{
    if(pos + len > memio->len)
    {
        memio->ok = false;
        len = memio->len - pos;
    }
    memcpy(buf, &memio->data[pos], len);
    return len;
}
size_t coy_memio_read_(struct coy_memio_* memio, void* buf, size_t len)
{
    size_t r = coy_memio_read_at_(memio, memio->pos, buf, len);
    memio->pos += len;
    return r;
}
uint32_t coy_memio_read32le_at_(struct coy_memio_* memio, size_t pos)
{
    uint32_t v;
    COY_CHECK(!(pos & (sizeof(v) - 1))); // check alignment
    if(pos + sizeof(v) > memio->len)
    {
        memio->ok = false;
        return 0;
    }
    v = 0;
    for(size_t i = 1; i <= sizeof(v); i++)
        v = (v << 8) | (uint32_t)memio->data[pos+sizeof(v)-i];
    return v;
}
uint32_t coy_memio_read32le_(struct coy_memio_* memio)
{
    uint32_t v = coy_memio_read32le_at_(memio, memio->pos);
    memio->pos += sizeof(v);
    return v;
}
uint64_t coy_memio_read64le_at_(struct coy_memio_* memio, size_t pos)
{
    uint64_t v;
    COY_CHECK(!(pos & (sizeof(v) - 1))); // check alignment
    if(pos + sizeof(v) > memio->len)
    {
        memio->ok = false;
        return 0;
    }
    v = 0;
    for(size_t i = 1; i <= sizeof(v); i++)
        v = (v << 8) | (uint32_t)memio->data[pos+sizeof(v)-i];
    return v;
}
uint64_t coy_memio_read64le_(struct coy_memio_* memio)
{
    uint64_t v = coy_memio_read64le_at_(memio, memio->pos);
    memio->pos += sizeof(v);
    return v;
}

// we'll eventually want to intern these, but for now ...
static char* coy_function_read_symbol_(struct coy_function2_* func, struct coy_memio_* memio, uint32_t pos, uint32_t len)
{
    char* sym = malloc(len + 1);
    if(coy_memio_read_at_(memio, pos + sizeof(len), sym, len) != len)
    {
        free(sym);
        return NULL;
    }
    sym[len] = 0;
    return sym;
}
static bool coy_function_read_consts_(struct coy_function2_* func, struct coy_memio_* memio)
{
    uint32_t nsymbols = coy_memio_read32le_(memio);
    uint32_t nrefs = coy_memio_read32le_(memio);
    uint32_t nvals = coy_memio_read32le_(memio);
    /*uint32_t _reserved1 = */coy_memio_read32le_(memio);
    if(!memio->ok) return false;
    func->u.coy.consts.nsymbols = nsymbols;
    func->u.coy.consts.nrefs = nrefs;
    func->u.coy.consts.data = NULL;
    stbds_arrsetlen(func->u.coy.consts.data, nsymbols + nrefs + nvals);
    for(size_t s = 0; s < nsymbols; s++)
    {
        uint32_t offset = coy_memio_read32le_(memio);
        uint32_t length = coy_memio_read32le_(memio);
        if(!memio->ok) return false;
        func->u.coy.consts.data[s].ptr = coy_function_read_symbol_(func, memio, offset, length);
    }
    if(!memio->ok) return false;
    for(size_t r = nsymbols; r < nsymbols + nrefs; r++)
        COY_TODO("loading reference constants");
    if(!memio->ok) return false;
    for(size_t v = nsymbols + nrefs; v < nsymbols + nrefs + nvals; v++)
        func->u.coy.consts.data[v].u64 = coy_memio_read64le_(memio);
    if(!memio->ok) return false;
    return true;
}
static bool coy_function_read_blocks_(struct coy_function2_* func, struct coy_memio_* memio)
{
    uint32_t nblocks = coy_memio_read32le_(memio);
    if(!memio->ok) return false;
    func->u.coy.blocks = NULL;
    stbds_arrsetlen(func->u.coy.blocks, nblocks);
    for(uint32_t b = 0; b < nblocks; b++)
    {
        struct coy_function2_block_* block = &func->u.coy.blocks[b];
        block->offset = coy_memio_read32le_(memio);
        block->nparams = coy_memio_read32le_(memio);
        uint32_t nptrs = coy_memio_read32le_(memio);
        if(!memio->ok) return false;
        block->ptrs = NULL;
        stbds_arrsetlen(block->ptrs, nptrs);
        for(uint32_t p = 0; p < nptrs; p++)
            block->ptrs[p] = coy_memio_read32le_(memio);
        if(!memio->ok) return false;
    }
    if(!memio->ok) return false;
    return true;
}
static bool coy_function_read_instrs_(struct coy_function2_* func, struct coy_memio_* memio)
{
    uint32_t ninstrs = coy_memio_read32le_(memio);
    if(!memio->ok) return false;
    func->u.coy.instrs = NULL;
    stbds_arrsetlen(func->u.coy.instrs, ninstrs);
    for(uint32_t i = 0; i < ninstrs; i++)
        // TODO: parse this correctly (because the bitfield might have a different order!)
        func->u.coy.instrs[i].raw = coy_memio_read32le_(memio);
    if(!memio->ok) return false;
    return true;
}
static void coy_function_compute_maxslots_(struct coy_function2_* func)
{
    uint32_t maxslots = 0;
    uint32_t nblocks = stbds_arrlenu(func->u.coy.blocks);
    for(uint32_t b = 0; b < nblocks; b++)
    {
        uint32_t coffset = func->u.coy.blocks[b+0].offset;
        uint32_t noffset = b + 1 < nblocks ? func->u.coy.blocks[b+1].offset : stbds_arrlenu(func->u.coy.instrs);
        uint32_t cslots = func->u.coy.blocks[b+0].nparams + (noffset - coffset);
        if(maxslots < cslots)
            maxslots = cslots;
    }
    func->u.coy.maxslots = maxslots ? maxslots - 1 : 0;
}

// TODO: maybe this should be elsewhere?
#define COY_VERIFY_FAIL_(...)   do { fprintf(stderr, "Function verify fail(%s:%d): ", __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); return false; } while(0)
#define COY_VERIFY_(test, ...)  do { if(!(test)) COY_VERIFY_FAIL_(__VA_ARGS__); } while(0)
#define COY_VERIFY_ARGIDX_(A)   COY_VERIFY_((A) < block->nparams + i, "argument tries to read from a future value %u", (A))
#define COY_VERIFY_REGVAL_(I)   COY_VERIFY_(!coy_bitarray_get(&isptr, block->nparams + (I)), "register %u: expected value type, found reference", (I))
#define COY_VERIFY_REGREF_(I)   COY_VERIFY_(coy_bitarray_get(&isptr, block->nparams + (I)), "register %u: expected reference type, found value", (I))
#define COY_VERIFY_ARGIDXA_(A)   COY_VERIFY_ARGIDX_(instr[1+(A)].arg.index)
#define COY_VERIFY_REGVALA_(A)   do { COY_VERIFY_ARGIDXA_(A); COY_VERIFY_REGVAL_(instr[1+(A)].arg.index); } while(0)
#define COY_VERIFY_REGREFA_(A)   do { COY_VERIFY_ARGIDXA_(A); COY_VERIFY_REGREF_(instr[1+(A)].arg.index); } while(0)

static bool coy_function_verify_jmp_block_(struct coy_function2_* func, const struct coy_function2_block_* block, const union coy_instruction_* instr, uint32_t i, coy_bitarray_t isptr, coy_bitarray_t jmpisptr, uint32_t jmpb, uint32_t ahead, uint32_t atail)
{
    uint32_t anum = atail - ahead;
    COY_VERIFY_(jmpb < stbds_arrlenu(func->u.coy.blocks), "jmp block index out of range");
    const struct coy_function2_block_* jmpblock = &func->u.coy.blocks[jmpb];
    COY_VERIFY_(anum == jmpblock->nparams, "invalid number of arguments for target block");
    coy_bitarray_clear(&jmpisptr);
    // populate `jmpisptr` for check
    for(size_t p = 0; p < stbds_arrlenu(block[0].ptrs); p++)
    {
        // we only check the parameters
        if(block[0].ptrs[p] >= jmpblock->nparams)
            continue;
        coy_bitarray_set(&jmpisptr, block[0].ptrs[p], true);
    }
    // ... now check that argument types match
    for(uint32_t a = 0; a < anum; a++)
    {
        // argument type must match jump destination type
        if(coy_bitarray_get(&jmpisptr, a))
            COY_VERIFY_REGREFA_(ahead+a);
        else
            COY_VERIFY_REGVALA_(atail+a);
    }
    return true;
}

static bool coy_function_verify_numeric_args_(struct coy_function2_* func, const struct coy_function2_block_* block, const union coy_instruction_* instr, uint32_t i, coy_bitarray_t isptr, coy_bitarray_t jmpisptr)
{
    for(uint32_t a = 0; a < instr->op.nargs; a++)
        COY_VERIFY_REGVALA_(a);
    return true;
}
static bool coy_function_verify_jmp_(struct coy_function2_* func, const struct coy_function2_block_* block, const union coy_instruction_* instr, uint32_t i, coy_bitarray_t isptr, coy_bitarray_t jmpisptr)
{
    return coy_function_verify_jmp_block_(func, block, instr, i, isptr, jmpisptr, instr[1].raw, 1, instr->op.nargs);
}
static bool coy_function_verify_jmpc_(struct coy_function2_* func, const struct coy_function2_block_* block, const union coy_instruction_* instr, uint32_t i, coy_bitarray_t isptr, coy_bitarray_t jmpisptr)
{
    COY_VERIFY_(instr->op.nargs >= 5, "jmpc needs two registers, two block indices, and separator");
    if(instr->op.flags & COY_OPFLG_POINTER)
    {
        COY_VERIFY_REGREFA_(0);
        COY_VERIFY_REGREFA_(1);
        switch(instr->op.flags & COY_OPFLG_CMP_MASK)
        {
        case COY_OPFLG_CMP_EQ:
        case COY_OPFLG_CMP_NE:
            // these are OK; no-op
            break;
        default:
            COY_VERIFY_FAIL_("reference value comparison can only be `==` or `!=`");
        }
    }
    else
    {
        COY_VERIFY_REGVALA_(0);
        COY_VERIFY_REGVALA_(1);
    }
    uint32_t jmpb_t = instr[3].raw;
    uint32_t jmpb_f = instr[4].raw;
    uint32_t jmpargsep = 5 + instr[5].raw;
    COY_VERIFY_(jmpargsep <= instr->op.nargs, "jmpc argument separator out of bounds");
    if(!coy_function_verify_jmp_block_(func, block, instr, i, isptr, jmpisptr, jmpb_t, 5, jmpargsep)) return false;
    if(!coy_function_verify_jmp_block_(func, block, instr, i, isptr, jmpisptr, jmpb_f, jmpargsep, instr->op.nargs)) return false;
    return true;
}
static bool coy_function_verify_ret_(struct coy_function2_* func, const struct coy_function2_block_* block, const union coy_instruction_* instr, uint32_t i, coy_bitarray_t isptr, coy_bitarray_t jmpisptr)
{
    // TODO: use typeinfo to verify that arg is correct (0 args for void, 1 val for val func, 1 ref for ref func)
    return true;
}

typedef bool coy_function_verify_helper_(struct coy_function2_* func, const struct coy_function2_block_* block, const union coy_instruction_* instr, uint32_t i, coy_bitarray_t isptr, coy_bitarray_t jmpisptr);
struct coy_function_verify_opinfo_
{
    // is the instruction's own register a reference?
    // - 0 if result must be value(*)
    // - 1 if it must be pointer
    // - -1 if we check this manually (e.g. because type depends)
    // (*NOTE: functions with no results MUST specify this as `0`)
    int8_t iref;
    // min and max number of arguments
    struct { uint32_t min, max; } args;
    // verification function; NULL here indicates an invalid instruction
    coy_function_verify_helper_* verify;

    // can this instruction be the last one in a block?
    bool islast;
};
static const struct coy_function_verify_opinfo_ coy_opinfos_[256] = {
    // arithmetic
    [COY_OPCODE_ADD] = {0,{2,2},coy_function_verify_numeric_args_},
    [COY_OPCODE_SUB] = {0,{2,2},coy_function_verify_numeric_args_},
    [COY_OPCODE_MUL] = {0,{2,2},coy_function_verify_numeric_args_},
    [COY_OPCODE_DIV] = {0,{2,2},coy_function_verify_numeric_args_},
    [COY_OPCODE_REM] = {0,{2,2},coy_function_verify_numeric_args_},
    // control flow
    [COY_OPCODE_JMP] = {0,{1,-1},coy_function_verify_jmp_,.islast=true},
    [COY_OPCODE_JMPC] = {0,{5,-1},coy_function_verify_jmpc_,.islast=true},
    [COY_OPCODE_RET] = {0,{0,1},coy_function_verify_ret_,.islast=true},
    // debugging
    [COY_OPCODE__DUMPU32] = {0,{1,-1},coy_function_verify_numeric_args_},
};

static bool coy_function_coy_verify_(struct coy_function2_* func)
{
    uint32_t nblocks = stbds_arrlenu(func->u.coy.blocks);
    uint32_t ninstrs = stbds_arrlenu(func->u.coy.instrs);
    coy_bitarray_t isptr;
    coy_bitarray_init(&isptr);
    coy_bitarray_setlen(&isptr, func->u.coy.maxslots);
    coy_bitarray_t jmpisptr;
    coy_bitarray_init(&jmpisptr);
    coy_bitarray_setlen(&jmpisptr, func->u.coy.maxslots);
    // first, we check the pointer offsets (this fact will be used for later checks)
    for(uint32_t b = 0; b < nblocks; b++)
    {
        const struct coy_function2_block_* block = &func->u.coy.blocks[b];
        uint32_t coffset = block[0].offset;
        uint32_t noffset = b + 1 < nblocks ? block[1].offset : ninstrs;
        COY_VERIFY_(coffset < noffset, "block starting offset must be before ending");
        COY_VERIFY_(coffset < ninstrs, "block offset out of bounds");
        uint32_t length = noffset - coffset;
        for(size_t p = 0; p < stbds_arrlenu(block->ptrs); p++)
        {
            COY_VERIFY_(block[0].ptrs[p] < block->nparams + length - 1, "block pointer offset out of bounds");
        }
    }
    for(uint32_t b = 0; b < nblocks; b++)
    {
        const struct coy_function2_block_* block = &func->u.coy.blocks[b];

        uint32_t coffset = block[0].offset;
        uint32_t noffset = b + 1 < nblocks ? block[1].offset : ninstrs;
        uint32_t length = noffset - coffset;

        coy_bitarray_clear(&isptr);
        for(size_t p = 0; p < stbds_arrlenu(block->ptrs); p++)
            coy_bitarray_set(&isptr, block->nparams + block->ptrs[p], true);

        const union coy_instruction_* instrs = &func->u.coy.instrs[block->offset];
        for(uint32_t i = 0; i < length; i += instrs[i].op.nargs)
        {
            const union coy_instruction_* instr = &instrs[i];
            COY_VERIFY_(i + instr->op.nargs <= length, "instruction arguments out of bounds");
            const struct coy_function_verify_opinfo_* opinfo = &coy_opinfos_[instr->op.code];
            COY_VERIFY_(opinfo->verify, "invalid instruction 0x%.2X", instr->op.code);
            if(i + instr->op.nargs == length)    //< if last instruction
            {
                COY_VERIFY_(opinfo->islast, "function block ends with invalid instruction");
            }
            if(!opinfo->verify(func, block, instr, i, isptr, jmpisptr)) return false;
            if(opinfo->iref > 0) COY_VERIFY_REGREF_(i);
            else if(opinfo->iref == 0) COY_VERIFY_REGVAL_(i);
            //else {} // we don't verify for iref<0 ("don't care" value)
        }
    }
    return true;
}

struct coy_function2_* coy_function_init_data_(struct coy_function2_* func, const struct coy_typeinfo_* type, uint32_t attrib, const void* data, size_t datalen)
{
    if(!func) return NULL;
    if(!COY_ENSURE(!(attrib & COY_FUNCTION_ATTRIB_NATIVE_), "misuse: cannot create a Coyote function with a `native` attribute"))
        return NULL;
    func->type = type;

    struct coy_memio_ memio = {
        .data = data,
        .pos = 0,
        .len = datalen,
        .ok = true,
    };
    if(!coy_function_read_consts_(func, &memio)) COY_TODO("free memory on error");
    if(!coy_function_read_blocks_(func, &memio)) COY_TODO("free memory on error");
    if(!coy_function_read_instrs_(func, &memio)) COY_TODO("free memory on error");
    coy_function_compute_maxslots_(func);
    if(!coy_function_coy_verify_(func)) COY_TODO("free memory on error");

    func->attrib = attrib;
    return func;
}
struct coy_function2_* coy_function_init_native_(struct coy_function2_* func, const struct coy_typeinfo_* type, uint32_t attrib, coy_c_function_t* handler, void* udata)
{
    if(!func) return NULL;
    if(!COY_ENSURE(handler, "misuse: cannot create a native function with a NULL handler"))
        return NULL;
    func->type = type;
    func->u.nat.handler = handler;
    func->u.nat.udata = udata;
    func->attrib = attrib | COY_FUNCTION_ATTRIB_NATIVE_;
    return func;
}

void coy_function_coy_compute_maxslots_(struct coy_function2_* func)
{
    if(!COY_ENSURE(!(func->attrib & COY_FUNCTION_ATTRIB_NATIVE_), "misuse: cannot compute maxslots on a `native` function"))
        return;
    coy_function_compute_maxslots_(func);
}
bool coy_function_verify_(struct coy_function2_* func)
{
    COY_VERIFY_(func->type, "function must have a type");
    if(func->attrib & COY_FUNCTION_ATTRIB_NATIVE_)
    {
        COY_VERIFY_(func->u.nat.handler, "native function is missing a handler");
        return true;
    }
    else
        return coy_function_coy_verify_(func);
}
