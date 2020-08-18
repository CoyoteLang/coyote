#include "function_builder.h"
#include "vm/register.h"
#include "util/string.h"
#include "util/debug.h"

#include "stb_ds.h"
#include <inttypes.h>

struct coy_function_builder_instr_ref_
{
    uint32_t block;
    uint32_t instr;
};
struct coy_function_builder_const_sym_entry_
{
    char* key;
    struct coy_function_builder_instr_ref_* value;
};
struct coy_function_builder_const_reg_entry_
{
    union coy_register_ key;
    struct coy_function_builder_instr_ref_* value;
};

struct coy_function_builder_* coy_function_builder_init_(struct coy_function_builder_* builder, const struct coy_typeinfo_* type, uint32_t attrib)
{
    if(!builder) return NULL;
    if(!coy_function_init_empty_(&builder->func, type, attrib)) return NULL;
    if(!COY_ENSURE(!(attrib & COY_FUNCTION_ATTRIB_NATIVE_), "misuse: cannot create a Coyote function builder with a `native` attribute"))
        return NULL;
    stbds_sh_new_arena(builder->consts.syms);
    builder->consts.refs = NULL;
    builder->consts.vals = NULL;
    builder->blocks = NULL;
    builder->curblock = UINT32_MAX;
    return builder;
}

static struct coy_function_builder_block_* coy_function_builder_curbblock_(struct coy_function_builder_* builder)
{
    COY_CHECK(builder->curblock < stbds_arrlenu(builder->blocks));
    return &builder->blocks[builder->curblock];
}
static struct coy_function_block_* coy_function_builder_curfblock_(struct coy_function_builder_* builder)
{
    COY_CHECK(builder->curblock < stbds_arrlenu(builder->func.u.coy.blocks));
    return &builder->func.u.coy.blocks[builder->curblock];
}
static void coy_function_builder_patch_constrefs_(struct coy_function_builder_* builder, const struct coy_function_builder_instr_ref_* constrefs, uint32_t cindex)
{
    // patch the references *to* the constant
    for(size_t i = 0; i < stbds_arrlenu(constrefs); i++)
    {
        COY_ASSERT(constrefs[i].block < stbds_arrlenu(builder->blocks));
        struct coy_function_builder_block_* block = &builder->blocks[constrefs[i].block];
        COY_ASSERT(constrefs[i].instr < stbds_arrlenu(block->instrs));
        COY_ASSERT(block->instrs[constrefs[i].instr].arg.isconst);
        block->instrs[constrefs[i].instr].arg.index = cindex;
    }
}
void coy_function_builder_finish_(struct coy_function_builder_* builder, struct coy_function_* func)
{
    // copy consts over
    struct coy_function_constants_* fconsts = &builder->func.u.coy.consts;
    size_t nsyms = stbds_shlenu(builder->consts.syms);
    size_t nrefs = stbds_hmlenu(builder->consts.refs);
    size_t nvals = stbds_hmlenu(builder->consts.vals);
    fconsts->nsymbols = nsyms;
    fconsts->nrefs = nrefs;
    stbds_arrsetlen(fconsts->data, nsyms + nrefs + nvals);
    for(size_t s = 0; s < nsyms; s++)
    {
        struct coy_function_builder_const_sym_entry_* entry = &builder->consts.syms[s];
        fconsts->data[s].ptr = coy_strdup_(entry->key, -1);
        coy_function_builder_patch_constrefs_(builder, entry->value, s);
    }
    for(size_t r = nsyms; r < nsyms + nrefs; r++)
    {
        struct coy_function_builder_const_reg_entry_* entry = &builder->consts.refs[r];
        fconsts->data[r] = entry->key;
        coy_function_builder_patch_constrefs_(builder, entry->value, r);
    }
    for(size_t v = nsyms + nrefs; v < nsyms + nrefs + nvals; v++)
    {
        struct coy_function_builder_const_reg_entry_* entry = &builder->consts.vals[v];
        fconsts->data[v] = entry->key;
        coy_function_builder_patch_constrefs_(builder, entry->value, v);
    }
    // compute memory needed for instructions
    size_t ninstrs = 0;
    for(size_t b = 0; b < stbds_arrlenu(builder->blocks); b++)
        ninstrs += stbds_arrlenu(builder->blocks[b].instrs);
    // copy instructions over
    stbds_arrsetlen(builder->func.u.coy.instrs, ninstrs);
    ninstrs = 0;
    for(size_t b = 0; b < stbds_arrlenu(builder->blocks); b++)
    {
        struct coy_function_builder_block_* bblock = &builder->blocks[b];
        struct coy_function_block_* fblock = &builder->func.u.coy.blocks[b];
        fblock->offset = ninstrs;
        size_t binstrs = stbds_arrlenu(bblock->instrs);
        memcpy(&builder->func.u.coy.instrs[ninstrs], bblock->instrs, binstrs * sizeof(union coy_instruction_));
        stbds_arrfree(bblock->instrs);
        ninstrs += binstrs;
    }
    // compute max # of slots
    coy_function_coy_compute_maxslots_(&builder->func);
    // return `func` (via parameter)
    *func = builder->func;
    // free own data
    stbds_shfree(builder->consts.syms);
    stbds_hmfree(builder->consts.refs);
    stbds_hmfree(builder->consts.vals);
    stbds_arrfree(builder->blocks);
}
void coy_function_builder_abort_(struct coy_function_builder_* builder)
{
    COY_TODO("free memory in coy_function_builder_abort_");
}

void coy_function_builder_useblock_(struct coy_function_builder_* builder, uint32_t block)
{
    COY_ASSERT(block < stbds_arrlenu(builder->func.u.coy.blocks));
    builder->curblock = block;
}
union coy_instruction_* coy_function_builder_curinstr_(struct coy_function_builder_* builder)
{
    struct coy_function_builder_block_* block = coy_function_builder_curbblock_(builder);
    COY_CHECK(block->curinstr < stbds_arrlenu(block->instrs));
    return &block->instrs[block->curinstr];
}

uint32_t coy_function_builder_block_(struct coy_function_builder_* builder, uint32_t nparams, const uint32_t* ptrs, size_t nptrs)
{
    for(size_t i = 0; i < nptrs; i++)
        if(!COY_ENSURE(ptrs[i] < nparams, "misuse: pointer parameter %" PRIu32 " out of bounds", ptrs[i]))
            return UINT32_MAX;
    // initialize builder block
    struct coy_function_builder_block_ bblock = {
        .instrs = NULL,
        .curinstr = UINT32_MAX,
    };
    stbds_arrput(builder->blocks, bblock);
    // initialize function block
    struct coy_function_block_ fblock = {
        .offset = stbds_arrlenu(builder->func.u.coy.instrs),
        .nparams = nparams,
        .ptrs = NULL,
    };
    stbds_arrsetlen(fblock.ptrs, nptrs);
    memcpy(fblock.ptrs, ptrs, nptrs * sizeof(*ptrs));
    stbds_arrput(builder->func.u.coy.blocks, fblock);
    // set current block & return it
    COY_ASSERT(stbds_arrlenu(builder->blocks) == stbds_arrlenu(builder->func.u.coy.blocks));
    builder->curblock = stbds_arrlenu(builder->blocks) - 1u;
    return builder->curblock;
}

uint32_t coy_function_builder_op_(struct coy_function_builder_* builder, uint8_t code, uint8_t flags, bool refresult)
{
    struct coy_function_builder_block_* bblock = coy_function_builder_curbblock_(builder);
    struct coy_function_block_* fblock = coy_function_builder_curfblock_(builder);
    bblock->curinstr = stbds_arrlenu(bblock->instrs);
    uint32_t dstreg = fblock->nparams + bblock->curinstr;
    union coy_instruction_ instr = {.op={code, flags, 0, 0}};
    stbds_arrput(bblock->instrs, instr);
    if(refresult) stbds_arrput(fblock->ptrs, dstreg);
    return dstreg;
}

void coy_function_builder_arg_reg_(struct coy_function_builder_* builder, uint32_t reg)
{
    struct coy_function_builder_block_* bblock = coy_function_builder_curbblock_(builder);
    ++coy_function_builder_curinstr_(builder)->op.nargs;
    union coy_instruction_ arg = {.arg={reg,0,false}};
    stbds_arrput(bblock->instrs, arg);
}
void coy_function_builder_arg_const_(struct coy_function_builder_* builder, union coy_register_ reg, uint8_t type)
{
    if(type == COY_FUNCTION_BUILDER_CONST_TYPE_SYM_)
    {
        coy_function_builder_arg_const_sym_(builder, reg.ptr);
        return;
    }
    struct coy_function_builder_block_* bblock = coy_function_builder_curbblock_(builder);
    struct coy_function_builder_const_reg_entry_** arr = type == COY_FUNCTION_BUILDER_CONST_TYPE_REF_ ? &builder->consts.refs : &builder->consts.vals;
#if 0   // stb_ds bug
    struct coy_function_builder_const_reg_entry_* entry = stbds_hmgetp_null(*arr, reg);
#else
    ptrdiff_t entryidx = stbds_hmgeti(*arr, reg);
    struct coy_function_builder_const_reg_entry_* entry = entryidx >= 0 ? &(*arr)[entryidx] : NULL;
#endif
    struct coy_function_builder_instr_ref_ constref = {
        .block = builder->curblock,
        .instr = stbds_arrlenu(bblock->instrs),
    };
    if(entry)
        stbds_arrput(entry->value, constref);
    else
    {
        struct coy_function_builder_const_reg_entry_ nentry = {reg, NULL};
        stbds_arrput(nentry.value, constref);
        stbds_hmputs(*arr, nentry);
    }
    // add a use pointer
    ++coy_function_builder_curinstr_(builder)->op.nargs;
    union coy_instruction_ arg = {.arg={(UINT32_C(1) << 23) - 1,0,true}};
    stbds_arrput(bblock->instrs, arg);
}
void coy_function_builder_arg_const_sym_(struct coy_function_builder_* builder, const char* sym)
{
    struct coy_function_builder_block_* bblock = coy_function_builder_curbblock_(builder);
#if 0   // stb_ds bug
    struct coy_function_builder_const_sym_entry_* entry = stbds_shgetp_null(builder->consts.syms, sym);
#else
    ptrdiff_t entryidx = stbds_shgeti(builder->consts.syms, sym);
    struct coy_function_builder_const_sym_entry_* entry = entryidx >= 0 ? &builder->consts.syms[entryidx] : NULL;
#endif
    struct coy_function_builder_instr_ref_ constref = {
        .block = builder->curblock,
        .instr = stbds_arrlenu(bblock->instrs),
    };
    if(entry)
        stbds_arrput(entry->value, constref);
    else
    {
        struct coy_function_builder_const_sym_entry_ nentry = {(char*)sym, NULL};
        stbds_arrput(nentry.value, constref);
        stbds_shputs(builder->consts.syms, nentry);
    }
    // add a use pointer
    ++coy_function_builder_curinstr_(builder)->op.nargs;
    union coy_instruction_ arg = {.arg={(UINT32_C(1) << 23) - 1,0,true}};
    stbds_arrput(bblock->instrs, arg);
}
void coy_function_builder_arg_const_ref_(struct coy_function_builder_* builder, void* ref)
{
    coy_function_builder_arg_const_(builder, (union coy_register_){.ptr=ref}, COY_FUNCTION_BUILDER_CONST_TYPE_REF_);
}
void coy_function_builder_arg_const_val_(struct coy_function_builder_* builder, union coy_register_ reg)
{
    coy_function_builder_arg_const_(builder, reg, COY_FUNCTION_BUILDER_CONST_TYPE_VAL_);
}
void coy_function_builder_arg_imm_(struct coy_function_builder_* builder, uint32_t imm)
{
    struct coy_function_builder_block_* bblock = coy_function_builder_curbblock_(builder);
    ++coy_function_builder_curinstr_(builder)->op.nargs;
    union coy_instruction_ arg = {.raw=imm};
    stbds_arrput(bblock->instrs, arg);
}
