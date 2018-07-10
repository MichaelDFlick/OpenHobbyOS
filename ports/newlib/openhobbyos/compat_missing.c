#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

extern char **environ;

/* Forward declarations for functions not in newlib headers */
int dup2(int oldfd, int newfd);
int execve(const char *path, char *const argv[], char *const envp[]);

/* Filesystem */
int statfs(const char *path, struct statfs *buf)
{
    (void)path;
    if (buf == NULL) { errno = EFAULT; return -1; }
    buf->f_type = 0;
    buf->f_bsize = 4096;
    buf->f_blocks = 0;
    buf->f_bfree = 0;
    buf->f_bavail = 0;
    buf->f_files = 0;
    buf->f_ffree = 0;
    buf->f_fsid = 0;
    buf->f_namelen = 255;
    buf->f_frsize = 4096;
    buf->f_flags = 0;
    return 0;
}

int fstatfs(int fd, struct statfs *buf)
{
    (void)fd;
    return statfs("/", buf);
}

/* System V semaphore / shared memory stubs */
key_t ftok(const char *path, int id)
{
    (void)path;
    return (key_t)(0x12340000 + id);
}

int shmget(key_t key, size_t size, int shmflg)
{
    (void)key;
    (void)size;
    (void)shmflg;
    errno = ENOSYS;
    return -1;
}

void *shmat(int shmid, const void *shmaddr, int shmflg)
{
    (void)shmid;
    (void)shmaddr;
    (void)shmflg;
    errno = ENOSYS;
    return (void *)-1;
}

int shmdt(const void *shmaddr)
{
    (void)shmaddr;
    errno = ENOSYS;
    return -1;
}

int shmctl(int shmid, int cmd, struct shmid_ds *buf)
{
    (void)shmid;
    (void)cmd;
    (void)buf;
    errno = ENOSYS;
    return -1;
}

/* pthread cleanup stubs (called directly by Qt) */
void _pthread_cleanup_push(void (*routine)(void*), void *arg)
{
    (void)routine;
    (void)arg;
}

void _pthread_cleanup_pop(int execute)
{
    (void)execute;
}

/* dup3 - close+dup2 with flags */
int dup3(int oldfd, int newfd, int flags)
{
    (void)flags;
    return dup2(oldfd, newfd);
}

/* vfork - same as fork in our environment */
pid_t vfork(void)
{
    errno = ENOSYS;
    return -1;
}

/* execv - execute program */
int execv(const char *path, char *const argv[])
{
    return execve(path, argv, environ);
}

/* fchdir - change directory by fd */
int fchdir(int fd)
{
    (void)fd;
    return 0;
}

/* flock - file locking */
int flock(int fd, int operation)
{
    (void)fd;
    (void)operation;
    return 0;
}

/* fdatasync - sync data */
int fdatasync(int fd)
{
    (void)fd;
    return 0;
}

/* truncate - truncate a file to a specified length */
int truncate(const char *path, off_t length)
{
    (void)path;
    (void)length;
    errno = ENOSYS;
    return -1;
}

/* unlinkat - remove directory entry relative to dir fd */
int unlinkat(int dirfd, const char *pathname, int flags)
{
    (void)dirfd;
    (void)pathname;
    (void)flags;
    errno = ENOSYS;
    return -1;
}

/* renameat - rename relative to directory fds */
int renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath)
{
    (void)olddirfd;
    (void)oldpath;
    (void)newdirfd;
    (void)newpath;
    errno = ENOSYS;
    return -1;
}

/* symlink - create symbolic link */
int symlink(const char *target, const char *linkpath)
{
    (void)target;
    (void)linkpath;
    errno = ENOSYS;
    return -1;
}

/* linkat - create hard link relative to directory fds */
int linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags)
{
    (void)olddirfd;
    (void)oldpath;
    (void)newdirfd;
    (void)newpath;
    (void)flags;
    errno = ENOSYS;
    return -1;
}

/* glibc fortified I/O stubs for libstdc++ */
#include <wchar.h>
#include <wctype.h>
#include <iconv.h>

int __fprintf_chk(FILE *stream, int flag, const char *format, ...)
{
    (void)flag;
    va_list ap;
    va_start(ap, format);
    int ret = vfprintf(stream, format, ap);
    va_end(ap);
    return ret;
}

size_t __mbsrtowcs_chk(wchar_t *dest, const char **src, size_t len, mbstate_t *ps, size_t dstlen)
{
    (void)dstlen;
    return mbsrtowcs(dest, src, len, ps);
}

wctype_t __wctype_l(const char *property, struct __locale_struct *locale)
{
    (void)locale;
    return wctype(property);
}

/* iconv stubs */
iconv_t iconv_open(const char *tocode, const char *fromcode)
{
    (void)tocode;
    (void)fromcode;
    return (iconv_t)-1;
}

size_t iconv(iconv_t cd, char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft)
{
    (void)cd;
    (void)inbuf;
    (void)inbytesleft;
    (void)outbuf;
    (void)outbytesleft;
    return (size_t)-1;
}

int iconv_close(iconv_t cd)
{
    (void)cd;
    return 0;
}

/* glibc locale stubs for libstdc++ */
struct __locale_struct;
char *__nl_langinfo_l(unsigned long item, struct __locale_struct *locale)
{
    (void)item;
    (void)locale;
    return "";
}

struct __locale_struct *__uselocale(struct __locale_struct *newloc)
{
    (void)newloc;
    return NULL;
}

/* Thread-Local Storage stub (single-threaded).
 * The argument is a tls_index { ti_module, ti_offset }.
 * We maintain a static TLS block to satisfy accesses. */
static char __tls_static_block[16384] __attribute__((aligned(16)));

void *___tls_get_addr(void *v)
{
    unsigned long *ti = v;
    unsigned long offset = ti[1]; /* ti_offset */
    if (offset >= sizeof(__tls_static_block))
        return __tls_static_block;
    return __tls_static_block + offset;
}

/* libstdc++ expects glibc's __cxa_thread_atexit_impl for TLS destructors */
int __cxa_thread_atexit_impl(void (*func)(void *), void *obj, void *dso_symbol)
{
    (void)func;
    (void)obj;
    (void)dso_symbol;
    return 0;
}

/* glibc extension: secure_getenv */
char *secure_getenv(const char *name)
{
    (void)name;
    return NULL;
}

/* C23 strtoul (same as plain strtoul) */
unsigned long __isoc23_strtoul(const char *restrict nptr, char **restrict endptr, int base)
{
    return strtoul(nptr, endptr, base);
}

/* glibc single-threaded hint */
const unsigned char __libc_single_threaded = 1;

/* libstdc++ expects stdout, stdin, stderr as global FILE* symbols. */
#undef stdout
#undef stdin
#undef stderr
FILE *stdout;
FILE *stdin;
FILE *stderr;

/* glibc __errno_location — newlib uses _REENT->_errno macro, not a function */
int *__errno_location(void)
{
    return &errno;
}

/* posix_memalign — aligned allocation */
int posix_memalign(void **memptr, size_t alignment, size_t size)
{
    if (memptr == NULL) return EINVAL;
    if (alignment < sizeof(void *)) alignment = sizeof(void *);
    /* alignment must be power of 2 and multiple of sizeof(void*) */
    if ((alignment & (alignment - 1)) != 0) return EINVAL;
    *memptr = malloc(size);
    if (*memptr == NULL) return ENOMEM;
    return 0;
}

/* glibc locale-aware wide char stubs */
wint_t __towupper_l(wint_t wc, struct __locale_struct *locale)
{
    (void)locale;
    return towupper(wc);
}

wint_t __towlower_l(wint_t wc, struct __locale_struct *locale)
{
    (void)locale;
    return towlower(wc);
}

int __iswctype_l(wint_t wc, wctype_t desc, struct __locale_struct *locale)
{
    (void)locale;
    return iswctype(wc, desc);
}

/* gettext / i18n stubs */
char *gettext(const char *msgid)
{
    return (char *)msgid;
}

char *dgettext(const char *domain, const char *msgid)
{
    (void)domain;
    return (char *)msgid;
}

char *bind_textdomain_codeset(const char *domain, const char *codeset)
{
    (void)domain;
    (void)codeset;
    return NULL;
}

/* glibc fortified I/O: __read_chk */
ssize_t __read_chk(int fd, void *buf, size_t count, size_t buflen)
{
    (void)buflen;
    return read(fd, buf, count);
}

/* glibc fortified wmem stubs */
wchar_t *__wmemset_chk(wchar_t *s, wchar_t c, size_t n, size_t dstlen)
{
    (void)dstlen;
    return wmemset(s, c, n);
}

wchar_t *__wmemcpy_chk(wchar_t *dest, const wchar_t *src, size_t n, size_t destlen)
{
    (void)destlen;
    return wmemcpy(dest, src, n);
}

/* pthread_rwlock stubs — types come from <sys/_pthreadtypes.h> via <sys/types.h> */
/* Note: NOT including <pthread.h> to avoid _pthread_cleanup_push/pop signature conflict */
struct timespec;
int pthread_rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr)
{
    (void)attr;
    if (rwlock == NULL) return EINVAL;
    *rwlock = 0;
    return 0;
}

int pthread_rwlock_destroy(pthread_rwlock_t *rwlock)
{
    (void)rwlock;
    return 0;
}

int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock)
{
    (void)rwlock;
    return 0;
}

int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock)
{
    (void)rwlock;
    return 0;
}

int pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock)
{
    (void)rwlock;
    return 0;
}

int pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock)
{
    (void)rwlock;
    return 0;
}

int pthread_rwlock_timedrdlock(pthread_rwlock_t *rwlock, const struct timespec *abstime)
{
    (void)rwlock;
    (void)abstime;
    return 0;
}

int pthread_rwlock_timedwrlock(pthread_rwlock_t *rwlock, const struct timespec *abstime)
{
    (void)rwlock;
    (void)abstime;
    return 0;
}

int pthread_rwlock_unlock(pthread_rwlock_t *rwlock)
{
    (void)rwlock;
    return 0;
}

int pthread_rwlockattr_init(pthread_rwlockattr_t *attr)
{
    if (attr == NULL) return EINVAL;
    memset(attr, 0, sizeof(*attr));
    return 0;
}

int pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr)
{
    (void)attr;
    return 0;
}

/* glibc locale management stubs — __*_l functions */
struct __locale_struct;
#include <locale.h>

struct __locale_struct *__newlocale(int category_mask, const char *locale, struct __locale_struct *base)
{
    (void)category_mask;
    (void)locale;
    (void)base;
    return NULL;
}

void __freelocale(struct __locale_struct *locale)
{
    (void)locale;
}

struct __locale_struct *__duplocale(struct __locale_struct *locale)
{
    (void)locale;
    return NULL;
}

unsigned int __ctype_get_mb_cur_max(void)
{
    return 6; /* UTF-8 max multibyte length */
}

/* glibc fortified mbsnrtowcs */
size_t __mbsnrtowcs_chk(wchar_t *dest, const char **src, size_t nms, size_t len, mbstate_t *ps, size_t dstlen)
{
    (void)dstlen;
    return mbsnrtowcs(dest, src, nms, len, ps);
}

/* locale-aware strtof/strtod */
float __strtof_l(const char *restrict nptr, char **restrict endptr, struct __locale_struct *locale)
{
    (void)locale;
    return strtof(nptr, endptr);
}

double __strtod_l(const char *restrict nptr, char **restrict endptr, struct __locale_struct *locale)
{
    (void)locale;
    return strtod(nptr, endptr);
}

/* locale-aware collation */
int __strcoll_l(const char *s1, const char *s2, struct __locale_struct *locale)
{
    (void)locale;
    return strcoll(s1, s2);
}

size_t __strxfrm_l(char *dest, const char *src, size_t n, struct __locale_struct *locale)
{
    (void)locale;
    return strxfrm(dest, src, n);
}

int __wcscoll_l(const wchar_t *s1, const wchar_t *s2, struct __locale_struct *locale)
{
    (void)locale;
    return wcscoll(s1, s2);
}

size_t __wcsxfrm_l(wchar_t *dest, const wchar_t *src, size_t n, struct __locale_struct *locale)
{
    (void)locale;
    return wcsxfrm(dest, src, n);
}

/* locale-aware time formatting */
#include <time.h>
size_t __strftime_l(char *s, size_t max, const char *fmt, const struct tm *tm, struct __locale_struct *locale)
{
    (void)locale;
    return strftime(s, max, fmt, tm);
}

size_t __wcsftime_l(wchar_t *s, size_t max, const wchar_t *fmt, const struct tm *tm, struct __locale_struct *locale)
{
    (void)locale;
    return wcsftime(s, max, fmt, tm);
}

/* Weak stubs for __swrite64 / __sseek64.
 * Newlib's findfp.o references these symbols, and the linker
 * processes libopenhobbyosgloss.a before libc.a, so the strong
 * implementations in compat_stdio64.c cannot satisfy libc.a's
 * references at link time.  These weak stubs provide the
 * necessary symbols; they are overridden at final link by the
 * strong definitions in compat_stdio64.o (when both are present). */
__attribute__((weak))
_READ_WRITE_RETURN_TYPE
__swrite64(struct _reent *reent, void *cookie, const char *buffer, _READ_WRITE_BUFSIZE_TYPE length)
{
    (void)reent;
    (void)cookie;
    (void)buffer;
    (void)length;
    return -1;
}

__attribute__((weak))
_fpos64_t
__sseek64(struct _reent *reent, void *cookie, _fpos64_t offset, int whence)
{
    (void)reent;
    (void)cookie;
    (void)offset;
    (void)whence;
    return (_fpos64_t)-1;
}

_off64_t lseek64(int fd, _off64_t offset, int whence)
{
    return lseek(fd, offset, whence);
}

int fstat64(int fd, struct stat64 *buf)
{
    return fstat(fd, (struct stat *)buf);
}

_off64_t _lseek64(int fd, _off64_t offset, int whence)
{
    return lseek(fd, offset, whence);
}

int _fstat64(int fd, struct stat64 *buf)
{
    return fstat(fd, (struct stat *)buf);
}

