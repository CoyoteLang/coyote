#include "bitarray.h"
#include "stb_ds.h"

#include <assert.h>
#include <string.h>
#include <limits.h>

coy_bitarray_t* coy_bitarray_init(coy_bitarray_t* bitarr)
{
    if(!bitarr) return NULL;
    bitarr->len = 0;
    bitarr->data = NULL;
    return bitarr;
}
void coy_bitarray_deinit(coy_bitarray_t* bitarr)
{
    stbds_arrfree(bitarr->data);
}
void coy_bitarray_clear(coy_bitarray_t* bitarr)
{
    memset(bitarr->data, 0, bitarr->len / CHAR_BIT);
}

size_t coy_bitarray_getlen_elems(const coy_bitarray_t* bitarr)
{
    return stbds_arrlenu(bitarr->data);
}
size_t coy_bitarray_get_elem(const coy_bitarray_t* bitarr, size_t elem)
{
    assert(elem < stbds_arrlenu(bitarr->data));
    return bitarr->data[elem];
}

size_t coy_bitarray_getlen(const coy_bitarray_t* bitarr)
{
    return bitarr->len;
}
void coy_bitarray_setlen(coy_bitarray_t* bitarr, size_t nlen)
{
    size_t plen_elems = stbds_arrlenu(bitarr->data);
    size_t nlen_elems = (nlen + COY_BITARRAY_BITS_PER_ELEMENT - 1) / COY_BITARRAY_BITS_PER_ELEMENT;
    if(plen_elems == nlen_elems)    // early exit
    {
        bitarr->len = nlen;
        return;
    }
    stbds_arrsetlen(bitarr->data, nlen_elems);
    if(nlen_elems > plen_elems)
        memset(&bitarr->data[plen_elems], 0, (nlen_elems - plen_elems) * sizeof(size_t));
    else if(nlen_elems && nlen < bitarr->len)
    {
        // we need to zero trailing bits
        size_t mask = ((size_t)1 << (nlen & (COY_BITARRAY_BITS_PER_ELEMENT - 1))) - 1;
        bitarr->data[nlen_elems-1] &= mask;
    }
    bitarr->len = nlen;
}

bool coy_bitarray_get(const coy_bitarray_t* bitarr, size_t bit)
{
    assert(bit < bitarr->len);
    size_t mask = (size_t)1 << (bit & (COY_BITARRAY_BITS_PER_ELEMENT - 1));
    return !!(bitarr->data[bit / COY_BITARRAY_BITS_PER_ELEMENT] & mask);
}
void coy_bitarray_set(coy_bitarray_t* bitarr, size_t bit, bool v)
{
    assert(bit < bitarr->len);
    size_t mask = (size_t)1 << (bit & (COY_BITARRAY_BITS_PER_ELEMENT - 1));
    if(v)
        bitarr->data[bit / COY_BITARRAY_BITS_PER_ELEMENT] |= mask;
    else
        bitarr->data[bit / COY_BITARRAY_BITS_PER_ELEMENT] &= ~mask;
}
size_t coy_bitarray_push(coy_bitarray_t* bitarr, bool v)
{
    size_t index = bitarr->len;
    coy_bitarray_setlen(bitarr, index + 1);
    coy_bitarray_set(bitarr, index, v);
    return index;
}
