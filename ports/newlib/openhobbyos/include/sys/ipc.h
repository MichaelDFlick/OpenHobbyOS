#ifndef _SYS_IPC_H
#define _SYS_IPC_H

#include <sys/cdefs.h>

__BEGIN_DECLS

#define IPC_CREAT  00001000
#define IPC_EXCL   00002000
#define IPC_NOWAIT 00004000
#define IPC_RMID   0
#define IPC_SET    1
#define IPC_STAT   2
#define IPC_PRIVATE 0
#define IPC_R      000400
#define IPC_W      000200
#define IPC_M      010000

struct ipc_perm {
    unsigned short uid;
    unsigned short gid;
    unsigned short cuid;
    unsigned short cgid;
    unsigned short mode;
    unsigned short seq;
    int key;
};

key_t ftok(const char *, int);

__END_DECLS

#endif
