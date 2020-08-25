#include "string.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "stb_ds.h"

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

coy_stringbuilder_t* coy_sb_init(coy_stringbuilder_t* sb)
{
    if(!sb) return NULL;
    sb->ptr = NULL;
    coy_sb_reset(sb);
    return sb;
}
void coy_sb_deinit(coy_stringbuilder_t* sb)
{
    if(!sb) return;
    stbds_arrfree(sb->ptr);
}
void coy_sb_reset(coy_stringbuilder_t* sb)
{
    stbds_arrsetlen(sb->ptr, 1);
    sb->ptr[0] = 0;
}

int coy_sb_putsl(coy_stringbuilder_t* sb, const char* str, size_t len)
{
    size_t plen = stbds_arrlenu(sb->ptr) - 1u;
    stbds_arrsetlen(sb->ptr, plen + len + 1u);
    memcpy(&sb->ptr[plen], str, len);
    sb->ptr[plen + len] = 0;
    return len;
}
int coy_sb_puts(coy_stringbuilder_t* sb, const char* str)
{
    return coy_sb_putsl(sb, str, strlen(str));
}
int coy_sb_putc(coy_stringbuilder_t* sb, int c)
{
    char cc = c;
    return coy_sb_putsl(sb, &cc, c >= 0);
}

int coy_sb_vprintf(coy_stringbuilder_t* sb, const char* format, va_list args)
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
        return len;
    size_t plen = stbds_arrlenu(sb->ptr) - 1u;
    stbds_arrsetlen(sb->ptr, plen + len + 1u);
    len = vsnprintf(&sb->ptr[plen], len + 1, format, args);
    if(len < 0)
        stbds_arrsetlen(sb->ptr, plen + 1u);
    return len;
}
int coy_sb_printf(coy_stringbuilder_t* sb, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int len = coy_sb_vprintf(sb, format, args);
    va_end(args);
    return len;
}

char* coy_sb_get(coy_stringbuilder_t* sb, size_t* len)
{
    if(len) *len = stbds_arrlenu(sb->ptr) - 1u;
    return sb->ptr;
}
