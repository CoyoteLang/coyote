#ifndef COY_TYPEINFO_H_
#define COY_TYPEINFO_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

struct coy_gc_;
struct coy_env;
typedef void coy_gc_mark_function_(struct coy_gc_* gc, void* ptr);
typedef void coy_gc_free_function_(struct coy_gc_* gc, void* ptr);
typedef void coy_gc_dtor_function_(struct coy_gc_* gc, void* ptr);
enum coy_typeinfo_category_
{
    COY_TYPEINFO_CAT_INTERNAL_,  //< should never, ever show up in user code
    COY_TYPEINFO_CAT_NORETURN_,
    COY_TYPEINFO_CAT_INTEGER_,
    COY_TYPEINFO_CAT_TENSOR_,
    COY_TYPEINFO_CAT_ARRAY_,
    //COY_TYPEINFO_CAT_SET_,
    //COY_TYPEINFO_CAT_ASSOC_ARRAY_,
    COY_TYPEINFO_CAT_FUNCTION_,
    //COY_TYPEINFO_CAT_TRAIT_,
    //COY_TYPEINFO_CAT_CLASS_,
};
struct coy_typeinfo_
{
    enum coy_typeinfo_category_ category;
    union
    {
        const char* internal_name;  // for debugging
        struct
        {
            bool is_signed;
            uint8_t width;
        } integer;
        struct
        {
            const struct coy_typeinfo_* basetype;
            const uint32_t* sizes;
            uint32_t ndims;
        } tensor;
        struct
        {
            const struct coy_typeinfo_* basetype;
            uint32_t ndims;
        } array;
        struct
        {
            const struct coy_typeinfo_* rtype;      //< return type
            const struct coy_typeinfo_** ptypes;    //< param types
            size_t nparams;
        } function;
    } u;
    coy_gc_mark_function_* cb_mark;
    coy_gc_free_function_* cb_free;
    coy_gc_dtor_function_* cb_dtor;
    const char* repr;   //< canonical representation
};

bool
#ifdef __GNUC__
__attribute__((deprecated("with interning, equality can be compared with `==`")))
#endif
coy_type_eql_(struct coy_typeinfo_ a, struct coy_typeinfo_ b);

struct coy_typeinfo_* coy_typeinfo_noreturn_(struct coy_env* env);
struct coy_typeinfo_* coy_typeinfo_integer_(struct coy_env* env, uint8_t width, bool is_signed);
struct coy_typeinfo_* coy_typeinfo_tensor_(struct coy_env* env, const struct coy_typeinfo_* basetype, const uint32_t* sizes, uint32_t ndims);
struct coy_typeinfo_* coy_typeinfo_array_(struct coy_env* env, const struct coy_typeinfo_* basetype, uint32_t ndims);
struct coy_typeinfo_* coy_typeinfo_function_(struct coy_env* env, const struct coy_typeinfo_* rtype, const struct coy_typeinfo_* const* ptypes, size_t nparams);

#endif /* COY_TYPEINFO_H_ */
