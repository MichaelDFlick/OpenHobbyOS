#ifndef FTSTDLIB_H_
#define FTSTDLIB_H_

#include <stddef.h>
#include <types.h>

#include <string.h>
#include <stdlib.h>

#define ft_ptrdiff_t  ptrdiff_t

#include <limits.h>

#define FT_CHAR_BIT    CHAR_BIT
#define FT_USHORT_MAX  USHRT_MAX
#define FT_INT_MAX     INT_MAX
#define FT_INT_MIN     INT_MIN
#define FT_UINT_MAX    UINT_MAX
#define FT_LONG_MIN    LONG_MIN
#define FT_LONG_MAX    LONG_MAX
#define FT_ULONG_MAX   ULONG_MAX

static inline void *ft_memchr(const void *s, int c, size_t n) {
    const unsigned char *p = (const unsigned char *)s;
    for (size_t i = 0; i < n; i++) if (p[i] == (unsigned char)c) return (void *)(p + i);
    return NULL;
}
#define ft_memcmp   memcmp
#define ft_memcpy   memcpy
#define ft_memmove  memmove
#define ft_memset   memset
static inline char *ft_strcat(char *dest, const char *src) {
    char *d = dest; while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}
#define ft_strcmp   strcmp
#define ft_strcpy   strcpy
#define ft_strlen   strlen
#define ft_strncmp  strncmp
#define ft_strncpy  strncpy
#define ft_strrchr  strrchr
static inline char *ft_strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack, *n = needle;
        while (*n && *h == *n) { h++; n++; }
        if (!*n) return (char *)haystack;
    }
    return NULL;
}

#undef FT_CONFIG_OPTION_USE_LONGJMP
#define ft_jmp_buf     int
#define ft_longjmp( x, y )  ((void)(x), (void)(y))
#define ft_setjmp( b )      0

#define ft_qsort( base, nmemb, size, cmp )  ((void)0)

#define ft_scalloc( n, size ) \
    ( ft_memset( malloc( (n) * (size) ), 0, (n) * (size) ) )
#define ft_sfree( p )        free( p )
#define ft_smalloc( size )   malloc( size )
#define ft_srealloc( p, s )  realloc( p, s )

#define ft_strtol( s, e, b )  0L
#define ft_getenv( x )       ((const char*)0)

#include <stdarg.h>

#endif
