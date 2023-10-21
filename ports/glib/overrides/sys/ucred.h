#ifndef _SYS_UCRED_H
#define _SYS_UCRED_H

#include <sys/types.h>

struct ucred {
    pid_t pid;
    uid_t uid;
    gid_t gid;
};

#endif
