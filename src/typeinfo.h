#ifndef COY_TYPEINFO_H_
#define COY_TYPEINFO_H_

#include <stdint.h>
#include <stdbool.h>

struct coy_gc_;
typedef void coy_gc_mark_function_(struct coy_gc_* gc, void* ptr);
typedef void coy_gc_free_function_(struct coy_gc_* gc, void* ptr);
typedef void coy_gc_dtor_function_(struct coy_gc_* gc, void* ptr);
enum coy_typeinfo_category_
{
    COY_TYPEINFO_CAT_INTERNAL_,  //< should never, ever show up in user code
    COY_TYPEINFO_CAT_NORETURN_,
    COY_TYPEINFO_CAT_INTEGER_,
    COY_TYPEINFO_CAT_FUNCTION_,
};
struct coy_typeinfo_
{
    enum coy_typeinfo_category_ category;
    union
    {
        const char* internal_name;  // for debugging
        struct
        {
            uint8_t is_signed;
            uint8_t width;
        } integer;
        struct
        {
            const struct coy_typeinfo_* rtype;  //< return type
             struct coy_typeinfo_** ptypes;//< param types
        } function;
    } u;
    coy_gc_mark_function_* cb_mark;
    coy_gc_free_function_* cb_free;
    coy_gc_dtor_function_* cb_dtor;
};

bool coy_type_eql_(struct coy_typeinfo_ a, struct coy_typeinfo_ b);

#endif
