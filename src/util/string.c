#include "string.h"

#include <stdlib.h>
#include <string.h>

char* coy_strdup_(const char* str, int len)
{
    if(len < 0) len = strlen(str);
    char* dup = malloc(len + 1);
    memcpy(dup, str, len);  // we *cannot* just do `len+1`, because there might not be a trailing \0
    dup[len] = 0;
    return dup;
}
