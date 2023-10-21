#ifndef _LINUX_RTNETLINK_H
#define _LINUX_RTNETLINK_H

#include <linux/netlink.h>
#include <stdint.h>

/* Routing netlink socket constants */

#define RTM_BASE 0x10
#define RTM_NEWROUTE    (RTM_BASE + 12)
#define RTM_DELROUTE    (RTM_BASE + 13)
#define RTM_GETROUTE    (RTM_BASE + 14)

#define RTN_UNSPEC      0
#define RTN_UNICAST     1
#define RTN_LOCAL       2
#define RTN_BROADCAST   3
#define RTN_ANYCAST     4
#define RTN_MULTICAST   5
#define RTN_BLACKHOLE   6
#define RTN_UNREACHABLE 7
#define RTN_PROHIBIT    8
#define RTN_THROW       9
#define RTN_NAT         10
#define RTN_XRESOLVE    11

#define RTMGRP_IPV4_ROUTE  0x80
#define RTMGRP_IPV6_ROUTE  0x100

/* rtattr - routing netlink attribute */
struct rtattr {
    unsigned short rta_len;
    unsigned short rta_type;
};

#define RTA_ALIGNTO     4
#define RTA_ALIGN(len)  (((len) + RTA_ALIGNTO - 1) & ~(RTA_ALIGNTO - 1))
#define RTA_LENGTH(len) (RTA_ALIGN(sizeof(struct rtattr)) + (len))
#define RTA_SPACE(len)  RTA_ALIGN(RTA_LENGTH(len))
#define RTA_DATA(rta)   ((void *)(((char *)(rta)) + RTA_LENGTH(0)))
#define RTA_PAYLOAD(rta) ((int)((rta)->rta_len) - RTA_LENGTH(0))
#define RTA_OK(rta, len) \
    ((len) >= (int)sizeof(struct rtattr) && \
     (rta)->rta_len >= sizeof(struct rtattr) && \
     (rta)->rta_len <= (len))
#define RTA_NEXT(rta, attrlen) \
    ((attrlen) -= RTA_ALIGN((rta)->rta_len), \
     (struct rtattr *)(((char *)(rta)) + RTA_ALIGN((rta)->rta_len)))
#define RTM_RTA(rth) \
    ((struct rtattr *)(((char *)(rth)) + NLMSG_ALIGN(sizeof(struct rtgenmsg))))

#define RTA_DST      1
#define RTA_GATEWAY  5
#define RTA_OIF      4

struct rtgenmsg {
    unsigned char rtgen_family;
};

struct rtmsg {
    unsigned char rtm_family;
    unsigned char rtm_dst_len;
    unsigned char rtm_src_len;
    unsigned char rtm_tos;
    unsigned char rtm_table;
    unsigned char rtm_protocol;
    unsigned char rtm_scope;
    unsigned char rtm_type;
    unsigned int  rtm_flags;
};

#endif
