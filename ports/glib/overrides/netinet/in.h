#ifndef _OVERRIDE_NETINET_IN_H
#define _OVERRIDE_NETINET_IN_H

#include_next <netinet/in.h>
#include <arpa/inet.h>

#ifndef IPV6_TCLASS
#define IPV6_TCLASS 67
#endif

#ifndef IPV6_RECVTCLASS
#define IPV6_RECVTCLASS 66
#endif

#ifndef IPV6_UNICAST_HOPS
#define IPV6_UNICAST_HOPS 16
#endif

#ifndef IPV6_MULTICAST_HOPS
#define IPV6_MULTICAST_HOPS 18
#endif

#ifndef IPV6_MULTICAST_IF
#define IPV6_MULTICAST_IF 17
#endif

#ifndef IPV6_MULTICAST_LOOP
#define IPV6_MULTICAST_LOOP 19
#endif

#ifndef IPV6_JOIN_GROUP
#define IPV6_JOIN_GROUP 20
#endif

#ifndef IPV6_LEAVE_GROUP
#define IPV6_LEAVE_GROUP 21
#endif

#ifndef IN_MULTICAST
#define IN_MULTICAST(a) ((((uint32_t)(a)) & 0xf0000000) == 0xe0000000)
#endif

#ifndef IN_LOOPBACK
#define IN_LOOPBACK(a) (((uint32_t)(a) & 0xff000000) == 0x7f000000)
#endif

#ifndef IN6_IS_ADDR_MULTICAST
#define IN6_IS_ADDR_MULTICAST(a) ((a)->s6_addr[0] == 0xff)
#endif

#ifndef IN6_IS_ADDR_SITELOCAL
#define IN6_IS_ADDR_SITELOCAL(a) (((a)->s6_addr[0] & 0xfe) == 0xfc)
#endif

#ifndef IN6_IS_ADDR_V4MAPPED
#define IN6_IS_ADDR_V4MAPPED(a) ((a)->s6_addr[0] == 0 && (a)->s6_addr[1] == 0 && \
    (a)->s6_addr[2] == 0 && (a)->s6_addr[3] == 0 && (a)->s6_addr[4] == 0 && \
    (a)->s6_addr[5] == 0 && (a)->s6_addr[6] == 0 && (a)->s6_addr[7] == 0 && \
    (a)->s6_addr[8] == 0 && (a)->s6_addr[9] == 0 && (a)->s6_addr[10] == 0xff && \
    (a)->s6_addr[11] == 0xff)
#endif

#ifndef IN6_IS_ADDR_MC_GLOBAL
#define IN6_IS_ADDR_MC_GLOBAL(a) (IN6_IS_ADDR_MULTICAST(a) && ((a)->s6_addr[1] & 0x0f) == 0x0e)
#endif

#ifndef IN6_IS_ADDR_MC_LINKLOCAL
#define IN6_IS_ADDR_MC_LINKLOCAL(a) (IN6_IS_ADDR_MULTICAST(a) && ((a)->s6_addr[1] & 0x0f) == 0x02)
#endif

#ifndef IN6_IS_ADDR_MC_NODELOCAL
#define IN6_IS_ADDR_MC_NODELOCAL(a) (IN6_IS_ADDR_MULTICAST(a) && ((a)->s6_addr[1] & 0x0f) == 0x01)
#endif

#ifndef IN6_IS_ADDR_MC_ORGLOCAL
#define IN6_IS_ADDR_MC_ORGLOCAL(a) (IN6_IS_ADDR_MULTICAST(a) && ((a)->s6_addr[1] & 0x0f) == 0x08)
#endif

#ifndef IN6_IS_ADDR_MC_SITELOCAL
#define IN6_IS_ADDR_MC_SITELOCAL(a) (IN6_IS_ADDR_MULTICAST(a) && ((a)->s6_addr[1] & 0x0f) == 0x05)
#endif

struct ip_mreq {
    struct in_addr imr_multiaddr;
    struct in_addr imr_interface;
};

struct ipv6_mreq {
    struct in6_addr ipv6mr_multiaddr;
    unsigned int    ipv6mr_interface;
};

#endif
