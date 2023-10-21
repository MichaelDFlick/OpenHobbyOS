#ifndef _OVERRIDE_SYS_SOCKET_H
#define _OVERRIDE_SYS_SOCKET_H

#include_next <sys/socket.h>
#include <sys/types.h>

#ifndef SOCK_SEQPACKET
#define SOCK_SEQPACKET 5
#endif

#ifndef SOMAXCONN
#define SOMAXCONN 128
#endif

#ifndef SOL_IP
#define SOL_IP 0
#endif

#ifndef SOL_IPV6
#define SOL_IPV6 41
#endif

struct ucred {
    pid_t pid;
    uid_t uid;
    gid_t gid;
};

#ifndef SO_PEERCRED
#define SO_PEERCRED 17
#endif

#ifndef SO_PASSCRED
#define SO_PASSCRED 16
#endif

#ifndef SCM_CREDENTIALS
#define SCM_CREDENTIALS 0x10
#endif

#ifndef MSG_TRUNC
#define MSG_TRUNC 0x20
#endif

#endif
