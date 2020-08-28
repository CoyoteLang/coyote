#ifndef COY_REGISTER_H_
#define COY_REGISTER_H_

#include <stdint.h>

// A register is a basic unit of storage.
union coy_register_
{
    int32_t i32;
    uint32_t u32;
    int64_t i64;
    uint64_t u64;
    float f32;
    double f64;
    void* ptr;
    // temporary
    struct
    {
        int32_t i32x2[2];
        uint32_t u32x2[2];
    } temp;
};

#endif /* COY_REGISTER_H_ */
