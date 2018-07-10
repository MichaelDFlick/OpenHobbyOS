#ifndef _BYTESWAP_H
#define _BYTESWAP_H

static __inline unsigned short bswap_16(unsigned short x)
{
    return (x >> 8) | (x << 8);
}

static __inline unsigned int bswap_32(unsigned int x)
{
    return __builtin_bswap32(x);
}

static __inline unsigned long long bswap_64(unsigned long long x)
{
    return __builtin_bswap64(x);
}

#endif
