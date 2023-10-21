/* Fix __locale_t conflict in C++ for newlib.
 * newlib defines different typedefs for C vs C++:
 *   C:      struct __locale_t;  typedef struct __locale_t *__locale_t, *locale_t;
 *   C++:    typedef void *__locale_t;  typedef void *locale_t;
 * This override matches the C++ version to prevent reent.h's
 * 'typedef void *__locale_t;' from conflicting.
 */
#ifndef _SYS__LOCALE_H
#define _SYS__LOCALE_H

#include <newlib.h>
#include <sys/config.h>

#ifdef __cplusplus
typedef void *__locale_t;
typedef void *locale_t;
#else
struct __locale_t;
typedef struct __locale_t *__locale_t;
typedef struct __locale_t *locale_t;
#endif

#endif	/* _SYS__LOCALE_H */
