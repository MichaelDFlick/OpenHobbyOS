#include <stddef.h>

void mbedtls_platform_zeroize(void *buf, size_t len)
{
    if (len > 0) {
        volatile unsigned char *p = (volatile unsigned char *)buf;
        for (size_t i = 0; i < len; i++) p[i] = 0;
    }
}
