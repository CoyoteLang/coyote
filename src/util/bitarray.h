#ifndef COY_BITARRAY_H_
#define COY_BITARRAY_H_

#include <stddef.h>
#include <stdbool.h>

#define COY_BITARRAY_BITS_PER_ELEMENT   (sizeof(size_t) * CHAR_BIT)

typedef struct coy_bitarray
{
    size_t len;
    size_t* data;
} coy_bitarray_t;

coy_bitarray_t* coy_bitarray_init(coy_bitarray_t* bitarr);
void coy_bitarray_deinit(coy_bitarray_t* bitarr);

// these operate in terms of elements (useful for iterating)
size_t coy_bitarray_getlen_elems(const coy_bitarray_t* bitarr);
size_t coy_bitarray_get_elem(const coy_bitarray_t* bitarr, size_t elem);

size_t coy_bitarray_getlen(const coy_bitarray_t* bitarr);
void coy_bitarray_setlen(coy_bitarray_t* bitarr, size_t nlen);

bool coy_bitarray_get(const coy_bitarray_t* bitarr, size_t bit);
void coy_bitarray_set(coy_bitarray_t* bitarr, size_t bit, bool v);
size_t coy_bitarray_push(coy_bitarray_t* bitarr, bool v);

#endif /* COY_BITARRAY_H_ */
