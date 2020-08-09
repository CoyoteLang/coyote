#include "string.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

char* coy_strdup_(const char* str, int len)
{
    if(len < 0) len = strlen(str);
    char* dup = malloc(len + 1);
    memcpy(dup, str, len);  // we *cannot* just do `len+1`, because there might not be a trailing \0
    dup[len] = 0;
    return dup;
}

char* coy_vaprintf_(const char* format, va_list args)
{
    va_list argcpy;
    va_copy(argcpy, args);
#ifdef _WIN32   // Windows' vsnprintf() is non-compliant, so we need this
    int len = _vscprintf(format, argcpy);
#else
    int len = vsnprintf(NULL, 0, format, argcpy);
#endif
    va_end(argcpy);
    if(len < 0)
        return NULL;
    char* ret = malloc(len + 1);
    len = vsnprintf(ret, len + 1, format, args);
    if(len < 0)
    {
        free(ret);
        return NULL;
    }
    return ret;
}
char* coy_aprintf_(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    char* ret = coy_vaprintf_(format, args);
    va_end(args);
    return ret;
}
