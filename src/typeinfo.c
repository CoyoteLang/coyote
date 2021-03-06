#include "typeinfo.h"
#include "vm/env.h"
#include "util/string.h"
#include "util/debug.h"

#include "stb_ds.h"
#include <inttypes.h>

static struct coy_typeinfo_* coy_env_get_typeinfo_by_repr_(coy_env_t* env, const char* repr)
{
#if 0   // stb_ds bug
    struct coy_typeinfo_entry_* entry = stbds_shgetp_null(env->typeinfos, repr);
#else
    ptrdiff_t entryidx = stbds_shgeti(env->typeinfos, repr);
    struct coy_typeinfo_entry_* entry = entryidx >= 0 ? &env->typeinfos[entryidx] : NULL;
#endif
    return entry ? entry->value : NULL;
}
static struct coy_typeinfo_* coy_env_intern_typeinfo_(coy_env_t* env, struct coy_typeinfo_* ti)
{
    struct coy_typeinfo_* current = coy_env_get_typeinfo_by_repr_(env, ti->repr);
    if(current)
    {
        free(ti);   //< we don't need a duplicate
        return current;
    }
    stbds_shput(env->typeinfos, ti->repr, ti);
    return ti;
}
static struct coy_typeinfo_* coy_env_intern_typeinfo_stack_(coy_env_t* env, const struct coy_typeinfo_* ti)
{
    struct coy_typeinfo_* current = coy_env_get_typeinfo_by_repr_(env, ti->repr);
    if(current)
        return current;
    current = malloc(sizeof(*ti));
    *current = *ti;
    stbds_shput(env->typeinfos, current->repr, current);
    return current;
}

struct coy_typeinfo_* coy_typeinfo_noreturn_(struct coy_env* env)
{
    struct coy_typeinfo_ ti = {
        .category=COY_TYPEINFO_CAT_NORETURN_,
        .repr="noreturn",
    };
    return coy_env_intern_typeinfo_stack_(env, &ti);
}
struct coy_typeinfo_* coy_typeinfo_integer_(struct coy_env* env, uint8_t width, bool is_signed)
{
    struct coy_typeinfo_ ti = {
        .category=COY_TYPEINFO_CAT_INTEGER_,
        .u={.integer={
            .width=width,
            .is_signed=is_signed,
        }},
    };
    struct
    {
        struct coy_typeinfo_ ti;
        char buf_repr[sizeof("ushort")];
    } *info = malloc(sizeof(*info));
    info->ti = ti;
    info->ti.repr = info->buf_repr;
    char* p = info->buf_repr;
    if(!is_signed) *p++ = 'u';
    switch(width)
    {
    case 8:     memcpy(p, "byte", sizeof("byte")); break;
    case 16:    memcpy(p, "short", sizeof("short")); break;
    case 32:    memcpy(p, "int", sizeof("int")); break;
    case 64:    memcpy(p, "long", sizeof("long")); break;
    case 128:   COY_ASSERT_MSG(false, "128-bit integers are not yet supported"); memcpy(p, "cent", sizeof("cent")); break;
    }
    return coy_env_intern_typeinfo_(env, &info->ti);
}
struct coy_typeinfo_* coy_typeinfo_tensor_(struct coy_env* env, const struct coy_typeinfo_* basetype, const uint32_t* sizes, uint32_t ndims)
{
    // TODO: allow this? (for allowing tensors of unknown sizes)
    COY_ASSERT_MSG(ndims, "cannot have a 0-dimensional tensor");
    const uint32_t* base_sizes;
    size_t base_ndims;
    // we intend to only support bool, integer, float, or tensor types
    switch(basetype->category)
    {
    case COY_TYPEINFO_CAT_INTEGER_:
        base_sizes = NULL;
        base_ndims = 0;
        break;
    case COY_TYPEINFO_CAT_TENSOR_:
        base_sizes = basetype->u.tensor.sizes;
        base_ndims = basetype->u.tensor.ndims;
        basetype = basetype->u.tensor.basetype;
        break;
    default:
        COY_ASSERT_MSG(false, "invalid base type for tensor");
        return NULL;
    }
    struct coy_typeinfo_ ti = {
        .category = COY_TYPEINFO_CAT_TENSOR_,
        .u={.tensor={
            .basetype=basetype,
            .ndims=base_ndims + ndims,
        }},
    };
    coy_stringbuilder_t sb;
    coy_sb_init(&sb);
    coy_sb_puts(&sb, basetype->repr);
    coy_sb_putc(&sb, '#');
    if(base_ndims + ndims > 1)
        coy_sb_putc(&sb, '[');
    size_t o = 0;
    for(size_t i = 0; i < base_ndims; i++)
    {
        if(o++) coy_sb_putc(&sb, ',');
        coy_sb_printf(&sb, "%" PRIu32, base_sizes[i]);
    }
    for(size_t i = 0; i < ndims; i++)
    {
        if(o++) coy_sb_putc(&sb, ',');
        coy_sb_printf(&sb, "%" PRIu32, sizes[i]);
    }
    if(base_ndims + ndims > 1)
        coy_sb_putc(&sb, ']');
    size_t sblen;
    char* sbptr = coy_sb_get(&sb, &sblen);
    struct
    {
        struct coy_typeinfo_ ti;
        uint32_t buf_dims[];
    } *info;
    size_t reproff = sizeof(*info) + sizeof(*info->buf_dims) * (base_ndims + ndims);
    char* infoc = malloc(reproff + sblen + 1);
    info = (void*)infoc;
    info->ti = ti;
    info->ti.u.tensor.sizes = info->buf_dims;
    info->ti.repr = &infoc[reproff];
    memcpy(&info->buf_dims[0], base_sizes, base_ndims * sizeof(*base_sizes));
    memcpy(&info->buf_dims[base_ndims], sizes, ndims * sizeof(*sizes));
    memcpy(&infoc[reproff], sbptr, sblen + 1u);
    coy_sb_deinit(&sb);
    return coy_env_intern_typeinfo_(env, &info->ti);
}
struct coy_typeinfo_* coy_typeinfo_array_(struct coy_env* env, const struct coy_typeinfo_* basetype, uint32_t ndims)
{
    COY_ASSERT_MSG(ndims, "cannot have a 0-dimensional array");
    struct coy_typeinfo_ ti = {
        .category = COY_TYPEINFO_CAT_ARRAY_,
        .u={.array={
            .basetype=basetype,
            .ndims=ndims,
        }},
    };
    size_t bt_reprlen = strlen(basetype->repr);
    struct
    {
        struct coy_typeinfo_ ti;
        char buf_repr[];
    } *info = malloc(sizeof(*info) + bt_reprlen + 3 + (ndims - 1u));
    info->ti = ti;
    info->ti.repr = info->buf_repr;
    memcpy(info->buf_repr, basetype->repr, bt_reprlen);
    char* p = info->buf_repr + bt_reprlen;
    *p++ = '[';
    while(--ndims)
        *p++ = ',';
    *p++ = ']';
    *p = 0;
    return coy_env_intern_typeinfo_(env, &info->ti);
}
struct coy_typeinfo_* coy_typeinfo_function_(struct coy_env* env, const struct coy_typeinfo_* rtype, const struct coy_typeinfo_* const* ptypes, size_t nparams)
{
    struct coy_typeinfo_ ti = {
        .category=COY_TYPEINFO_CAT_FUNCTION_,
        .u={.function={
            .rtype=rtype,
            .nparams=nparams,
        }},
    };
    struct
    {
        struct coy_typeinfo_ ti;
        const struct coy_typeinfo_* buf_ptypes[];
    } *info;
    coy_stringbuilder_t sb;
    coy_sb_init(&sb);
    coy_sb_puts(&sb, rtype->repr);
    coy_sb_putc(&sb, '(');
    for(size_t p = 0; p < nparams; p++)
    {
        if(p) coy_sb_putc(&sb, ',');
        coy_sb_puts(&sb, ptypes[p]->repr);
    }
    coy_sb_putc(&sb, ')');
    size_t reprlen;
    char* repr = coy_sb_get(&sb, &reprlen);
    size_t reproff = sizeof(*info) + sizeof(*info->buf_ptypes) * (nparams + /*compat*/1);
    char* infoc = malloc(reproff + reprlen + 1);
    info = (void*)infoc;
    info->ti = ti;
    info->ti.u.function.ptypes = info->buf_ptypes;
    info->ti.repr = &infoc[reproff];
    memcpy(info->ti.u.function.ptypes, ptypes, nparams * sizeof(const struct coy_typeinfo_*));
    info->ti.u.function.ptypes[nparams] = NULL; //< compat
    memcpy(&infoc[reproff], repr, reprlen + 1);
    coy_sb_deinit(&sb);
    return coy_env_intern_typeinfo_(env, &info->ti);
}
