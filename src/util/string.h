#ifndef COY_UTIL_STRING_H_
#define COY_UTIL_STRING_H_

#include <stdarg.h>

char* coy_strdup_(const char* str, int len);

char* coy_vaprintf_(const char* format, va_list args);
char* coy_aprintf_(const char* format, ...);

#endif /* COY_UTIL_STRING_H_ */
