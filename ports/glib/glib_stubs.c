#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utime.h>
#include <sys/time.h>
#include <stdarg.h>
#include <errno.h>
#include <ftw.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>

/* utime - wrapper around utimes */
int utime(const char *path, const struct utimbuf *times) {
    if (times) {
        struct timeval tv[2];
        tv[0].tv_sec = (time_t)times->actime;
        tv[0].tv_usec = 0;
        tv[1].tv_sec = (time_t)times->modtime;
        tv[1].tv_usec = 0;
        return utimes(path, tv);
    }
    return utimes(path, NULL);
}

/* syslog stubs */
void openlog(const char *ident, int option, int facility) { (void)ident; (void)option; (void)facility; }
void syslog(int priority, const char *format, ...) { (void)priority; (void)format; }
void closelog(void) {}

/* creat - wrapper around open */
int creat(const char *path, mode_t mode) { return open(path, O_WRONLY | O_CREAT | O_TRUNC, mode); }

/* nftw stub - not implemented */
int nftw(const char *dirpath, int (*fn)(const char *, const struct stat *, int, struct FTW *), int nopenfd, int flags) {
    (void)dirpath; (void)fn; (void)nopenfd; (void)flags;
    errno = ENOSYS;
    return -1;
}

int getnameinfo(const struct sockaddr *addr, socklen_t addrlen, char *host, socklen_t hostlen, char *serv, socklen_t servlen, int flags) {
    (void)addr; (void)addrlen; (void)host; (void)hostlen; (void)serv; (void)servlen; (void)flags;
    return EAI_NONAME;
}
