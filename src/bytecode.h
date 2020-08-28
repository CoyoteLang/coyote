#ifndef COY_BYTECODE_H_
#define COY_BYTECODE_H_

#include <stdint.h>

#include "vm/compat_shims.h"

typedef struct coy_compat_module_
{
    const struct coy_refval_vtbl_* vtbl;
    union coy_register_* variables;
    struct coy_function_ *functions;
} coy_module_t;

void coy_module_free(coy_module_t *module);

#define COY_ENUM_instruction_opcode_(ITEM,VAL,VLAST)    \
    ITEM(NOP),              \
    ITEM(ADD),              \
    ITEM(SUB),              \
    ITEM(MUL),              \
    ITEM(DIV),              \
    ITEM(REM),              \
    ITEM(JMP),              \
    ITEM(JMPC),             \
    ITEM(CALL),             \
    ITEM(RETCALL),          \
    ITEM(RET),              \
    ITEM(_DUMPU32)VAL(0xF0) \
    VLAST(0xFF)

/*
jmp encoding for `jmp .X($1,$2):
    OP:     op=jmp
    ARG:    immediate=.X
    ARG:    <arbitrary amount of pairs of moves>

jmpc encoding for `jmpc lt $a, $b, .X($1,$2), .Y($3,$4)`:
    OP:     op=jmpc flags=TYPE_x | CMP_LT               ; CMP is one of EQ, NE, LT, LE
    ARG:    reg=$a
    ARG:    reg=$b
    ARG:    immediate=.X                                ; encoded directly (not a reg)
    ARG:    immediate=.Y                                ; encoded directly (not a reg)
    ARG:    immediate=moves_sep                         ; end of .X, start of .Y
    ARG:    <arbitrary amount of pairs of moves>
    ; .X's parameters are in args: [5,5+moves_sep)
    ; .Y's parameters are in args: [5+moves_sep,nargs)
*/

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

#define COY_OPFLG_TYPE_INT32X2_TEMP     (COY_OPFLG_SIGNED | 0x04)
#define COY_OPFLG_TYPE_UINT32X2_TEMP    (COY_OPFLG_UNSIGNED | 0x04)
#define COY_OPFLG_TYPE_MASK     (0xC0 | 0x30 | /*temp*/ 0x04)

// used in JMP
#define COY_OPFLG_CMP_EQ    0x00
#define COY_OPFLG_CMP_NE    0x01
#define COY_OPFLG_CMP_LE    0x02
#define COY_OPFLG_CMP_LT    0x03
// use LT, LE respectively (with reversed operands)
//#define COY_OPFLG_CMP_GE
//#define COY_OPFLG_CMP_GT

#define COY_OPFLG_CMP_MASK  0x03

#endif /* COY_BYTECODE_H_ */
