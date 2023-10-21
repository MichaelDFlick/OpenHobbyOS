#ifndef _OVERRIDE_ARPA_INET_H
#define _OVERRIDE_ARPA_INET_H

#include_next <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>

#ifndef INADDR_NONE
#define INADDR_NONE ((in_addr_t)-1)
#endif

#ifndef inet_addr
static inline in_addr_t inet_addr(const char *cp) {
    unsigned int a,b,c,d;
    if (sscanf(cp, "%u.%u.%u.%u", &a, &b, &c, &d) != 4)
        return INADDR_NONE;
    if (a > 255 || b > 255 || c > 255 || d > 255)
        return INADDR_NONE;
    return (a << 24) | (b << 16) | (c << 8) | d;
}
#endif

#ifndef inet_aton
static inline int inet_aton(const char *cp, struct in_addr *inp) {
    in_addr_t result = inet_addr(cp);
    if (result == INADDR_NONE && strcmp(cp, "255.255.255.255") != 0)
        return 0;
    inp->s_addr = result;
    return 1;
}
#endif

#endif
