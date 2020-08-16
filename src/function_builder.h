#ifndef COY_FUNCTION_BUILDER_H_
#define COY_FUNCTION_BUILDER_H_

#include "vm/function2.h"

#define COY_FUNCTION_BUILDER_CONST_TYPE_SYM_    0
#define COY_FUNCTION_BUILDER_CONST_TYPE_REF_    1
#define COY_FUNCTION_BUILDER_CONST_TYPE_VAL_    2

struct coy_function_builder_const_sym_entry_;
struct coy_function_builder_const_reg_entry_;

struct coy_function_builder_block_
{
    union coy_instruction_* instrs;
    uint32_t curinstr;  //< index of start of last instruction
};
struct coy_function_builder_
{
    struct coy_function2_ func;
    struct
    {
        struct coy_function_builder_const_sym_entry_* syms;
        struct coy_function_builder_const_reg_entry_* refs;
        struct coy_function_builder_const_reg_entry_* vals;
    } consts;
    struct coy_function_builder_block_* blocks;
    uint32_t curblock;  //< current "active" block
};
struct coy_function_builder_* coy_function_builder_init_(struct coy_function_builder_* builder, const struct coy_typeinfo_* type, uint32_t attrib);
void coy_function_builder_finish_(struct coy_function_builder_* builder, struct coy_function2_* func);
void coy_function_builder_abort_(struct coy_function_builder_* builder);

void coy_function_builder_useblock_(struct coy_function_builder_* builder, uint32_t block);
struct coy_function_builder_block_* coy_function_builder_curbblock_(struct coy_function_builder_* builder);
struct coy_function2_block_* coy_function_builder_curfblock_(struct coy_function_builder_* builder);
union coy_instruction_* coy_function_builder_curinstr_(struct coy_function_builder_* builder);

uint32_t coy_function_builder_block_(struct coy_function_builder_* builder, uint32_t nparams, const uint32_t* ptrs, size_t nptrs);

uint32_t coy_function_builder_op_(struct coy_function_builder_* builder, uint8_t code, uint8_t flags, bool refresult);

void coy_function_builder_arg_reg_(struct coy_function_builder_* builder, uint32_t reg);
void coy_function_builder_arg_const_(struct coy_function_builder_* builder, union coy_register_ reg, uint8_t type);
void coy_function_builder_arg_const_sym_(struct coy_function_builder_* builder, const char* sym);
void coy_function_builder_arg_const_ref_(struct coy_function_builder_* builder, void* ref);
void coy_function_builder_arg_const_val_(struct coy_function_builder_* builder, union coy_register_ reg);
void coy_function_builder_arg_imm_(struct coy_function_builder_* builder, uint32_t imm);

#endif /* COY_FUNCTION_BUILDER_H_ */
