#ifndef COY_UTIL_STRING_H_
#define COY_UTIL_STRING_H_

#include <stddef.h>
#include <stdarg.h>

char* coy_strdup_(const char* str, int len);

char* coy_vaprintf_(const char* format, va_list args);
char* coy_aprintf_(const char* format, ...);

typedef struct coy_stringbuilder
{
    char* ptr;
} coy_stringbuilder_t;

coy_stringbuilder_t* coy_sb_init(coy_stringbuilder_t* sb);
void coy_sb_deinit(coy_stringbuilder_t* sb);
void coy_sb_reset(coy_stringbuilder_t* sb);

int coy_sb_putsl(coy_stringbuilder_t* sb, const char* str, size_t len);
int coy_sb_puts(coy_stringbuilder_t* sb, const char* str);
int coy_sb_putc(coy_stringbuilder_t* sb, int c);

int coy_sb_vprintf(coy_stringbuilder_t* sb, const char* format, va_list args);
int coy_sb_printf(coy_stringbuilder_t* sb, const char* format, ...);

char* coy_sb_get(coy_stringbuilder_t* sb, size_t* len);

#endif /* COY_UTIL_STRING_H_ */
