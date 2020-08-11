#ifndef COY_VM_TYPEINFO_H_
#define COY_VM_TYPEINFO_H_

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

#endif /* COY_VM_TYPEINFO_H_ */
