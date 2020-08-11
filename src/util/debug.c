#include "debug.h"

#include <stdio.h>
#include <stdlib.h>

bool coy_vensure_(bool v, const char* file, int line, const char* format, va_list args)
{
#if 1   // debug mode
    if(v)
        return true;
    fputs("COY_ENSURE failed: ", stderr);
    vfprintf(stderr, format, args);
    putc('\n', stderr);
    abort();
    return false;
#elif 0 // coverage testing mode
    return true;
#else   // release mode
    return v;
#endif
}

bool coy_ensure_(bool v, const char* file, int line, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    v = coy_vensure_(v, file, line, format, args);
    va_end(args);
    return v;
}
