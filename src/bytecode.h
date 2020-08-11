#ifndef COY_BYTECODE_H_
#define COY_BYTECODE_H_

#include <stdint.h>

#include "vm/vm.h"

typedef union coy_instruction_
{
    struct
    {
        uint8_t code;
        uint8_t flags;
        uint8_t _reserved;  // eventually: VM nregister?
        uint8_t nargs;
    } op;
    struct
    {
        uint32_t index: 24;
        uint32_t _reserved : 8; // eventually: VM register?
    } arg;
    uint32_t raw;
} coy_instruction_t;

// This can eventually be merged with `instrs`, to avoid multiple allocations.
typedef struct coy_function_block_
{
    uint32_t nparams;   //< # of a parameters in a block
    uint32_t offset;    //< block's starting offset
} coy_function_block_t;

typedef int32_t coy_c_function_t(coy_thread_t* thread);

typedef struct coy_function_
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
} coy_function_t;

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

typedef struct coy_module_
{
    const struct coy_refval_vtbl_* vtbl;
    union coy_register_* variables;
    coy_function_t *functions;
} coy_module_t;

void coy_module_free(coy_module_t *module);

#define COY_ENUM_instruction_opcode_(ITEM,VAL,VLAST)    \
    ITEM(NOP),              \
    ITEM(ADD),              \
    ITEM(SUB),              \
    ITEM(MUL),              \
    ITEM(DIV),              \
    ITEM(REM),              \
    /*ITEM(JMP),              */\
    ITEM(RET),              \
    ITEM(_DUMPU32)VAL(0xF0) \
    VLAST(0xFF)

typedef enum coy_instruction_opcode_
{
#define COY_ITEM_(NAME)    COY_OPCODE_##NAME
#define COY_VAL_(V)        = (V)
#define COY_VLAST_(V)      ,COY_OPCODE_LAST_ = (V)
    COY_ENUM_instruction_opcode_(COY_ITEM_,COY_VAL_,COY_VLAST_)
#undef COY_ITEM_
#undef COY_VAL_
#undef COY_VLAST_
} coy_instruction_opcode_t;

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

#endif /* COY_BYTECODE_H_ */
