#ifndef COY_UTIL_DEBUG_H_
#define COY_UTIL_DEBUG_H_

#include "hints.h"

#include <stdbool.h>
#include <stdarg.h>
#include <assert.h>

bool COY_HINT_PRINTF(4,0) coy_vensure_(bool v, const char* file, int line, const char* format, va_list args);
bool COY_HINT_PRINTF(4,5) coy_ensure_(bool v, const char* file, int line, const char* format, ...);

/*
    ASSERT: Something we expect to *always* be true, even considering bugs.
    Should be 100% certain (otherwise, use CHECK or ENSURE): A failure should *always* indicate something akin to memory corruption.
    - debug: asserts `x`
    - coverage testing: no-op
    - release: no-op
*/
#define COY_ASSERT(x)               assert(x)
#define COY_ASSERT_MSG(x, message)  assert((x) && message)
/*
    CHECK: Essentially an ENSURE that we cannot handle gracefully. Unlike ASSERT, this will (eventually) *not* abort the entire program, but merely the VM.
    - debug: abort() if `x` is false (but eventually: only abort VM)
    - coverage testing: no-op
    - release: simply returns `x`
*/
#define COY_CHECK(x, message)       assert(x)
#define COY_CHECK_MSG(x, message)   assert((x) && message)
/*
    UNREACHABLE:
    - debug: abort()
    - coverage testing: abort() (because reaching it means a faulty test or a definitive bug)
    - release: undefined behaviour
*/
#define COY_UNREACHABLE()       abort()
// A special case of COY_UNREACHABLE for TODOs (the reason is to allow easy grepping)
#define COY_TODO(message)       assert(0 && "TODO: " message)
/*
    ENSURE: Something we expect to always be true, but cannot prove it.
    - debug: abort() if `x` is false
    - coverage testing: returns constant `true`
    - release: simply returns `x`
*/
#define COY_ENSURE(x, ...)      coy_ensure_(!!(x), __FILE__, __LINE__, __VA_ARGS__)

#endif /* COY_UTIL_DEBUG_H_ */
