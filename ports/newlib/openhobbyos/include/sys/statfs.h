#ifndef _SYS_STATFS_H
#define _SYS_STATFS_H

#include <sys/types.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

struct statfs {
    long f_type;
    long f_bsize;
    fsblkcnt_t f_blocks;
    fsblkcnt_t f_bfree;
    fsblkcnt_t f_bavail;
    fsfilcnt_t f_files;
    fsfilcnt_t f_ffree;
    long f_fsid;
    long f_namelen;
    long f_frsize;
    long f_flags;
    long f_spare[4];
};

#define statfs64 statfs
#define fstatfs64 fstatfs

#define ST_RDONLY 0x0001
#define ST_NOSUID 0x0002

int statfs(const char *path, struct statfs *buf);
int fstatfs(int fd, struct statfs *buf);

__END_DECLS

#endif
