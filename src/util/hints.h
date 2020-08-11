#ifndef COY_UTIL_HINTS_H_
#define COY_UTIL_HINTS_H_

#ifdef noreturn
#define COY_HINT_NORETURN noreturn
#elif __STDC_VERSION__ >= 201112L
#define COY_HINT_NORETURN _Noreturn
#elif defined(__GNUC__)
#define COY_HINT_NORETURN __attribute__((noreturn))
#elif defined(_MSC_VER)
#define COY_HINT_NORETURN __declspec(noreturn)
#else
#define COY_HINT_NORETURN
#endif

#ifdef __GNUC__
#define COY_HINT_PRINTF(STRING_INDEX, FIRST_TO_CHECK) __attribute__((format(printf,STRING_INDEX,FIRST_TO_CHECK)))
#else
#define COY_HINT_PRINTF(STRING_INDEX, FIRST_TO_CHECK)
#endif

#endif /* COY_UTIL_HINTS_H_ */
