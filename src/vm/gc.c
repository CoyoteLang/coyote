#include "gc.h"
#include "../typeinfo.h"
#include "../util/debug.h"

#include "stb_ds.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <inttypes.h>

struct coy_gcobj_
{
    const struct coy_typeinfo_* typeinfo;           // type information
    size_t index : sizeof(size_t) * CHAR_BIT - 2;   //< index in set
    size_t set : 2;                                 //< active set (2==thread root, 3==common root)
};
struct coy_gc_* coy_gc_init_(struct coy_gc_* gc)
{
    if(!gc) return NULL;
    for(size_t i = 0; i < sizeof(gc->sets) / sizeof(*gc->sets); i++)
        gc->sets[i] = NULL;
    gc->curset = 0;
    return gc;
}
void coy_gc_deinit_(struct coy_gc_* gc)
{
    if(!gc) return;
    // run all destructors
    for(size_t s = 0; s < sizeof(gc->sets) / sizeof(*gc->sets); s++)
    {
        for(size_t i = 0; i < stbds_arrlenu(gc->sets[s]); i++)
        {
            struct coy_gcobj_* obj = gc->sets[s][i];
            if(obj->typeinfo->cb_dtor)
                obj->typeinfo->cb_dtor(gc, obj + 1);
        }
    }
    // now free memory
    for(size_t s = 0; s < sizeof(gc->sets) / sizeof(*gc->sets); s++)
    {
        for(size_t i = 0; i < stbds_arrlenu(gc->sets[s]); i++)
        {
            struct coy_gcobj_* obj = gc->sets[s][i];
            if(obj->typeinfo->cb_free)
                obj->typeinfo->cb_free(gc, obj + 1);
        }
        stbds_arrfree(gc->sets[s]);
    }
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

void coy_gc_mark_(struct coy_gc_* gc, void* ptr)
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
