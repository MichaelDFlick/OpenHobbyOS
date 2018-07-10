#ifndef _SYS_SHM_H
#define _SYS_SHM_H

#include <sys/cdefs.h>
#include <sys/ipc.h>
#include <sys/types.h>

__BEGIN_DECLS

#define SHM_RDONLY  010000
#define SHM_RND     020000
#define SHM_REMAP   040000
#define SHM_R       000400
#define SHM_W       000200
#define SHM_LOCK    11
#define SHM_UNLOCK  12

typedef unsigned long shmatt_t;

struct shmid_ds {
    struct ipc_perm shm_perm;
    size_t shm_segsz;
    pid_t shm_lpid;
    pid_t shm_cpid;
    shmatt_t shm_nattch;
    time_t shm_atime;
    time_t shm_dtime;
    time_t shm_ctime;
};

void *shmat(int, const void *, int);
int shmctl(int, int, struct shmid_ds *);
int shmdt(const void *);
int shmget(key_t, size_t, int);

__END_DECLS

#endif
