#ifndef OPENHOBBYOS_FEATURES_H
#define OPENHOBBYOS_FEATURES_H

/*
 * A few Linux-facing ports probe GNU open flags directly. Newlib doesn't
 * advertise O_PATH for our target, so we reserve the same bit Cygwin uses and
 * translate it when requests cross into the kernel ABI.
 */
#ifndef O_PATH
#define O_PATH 0x02000000
#endif

/* libstdc++ (glibc build) expects __GLIBC_PREREQ from features.h.
   Guard on __GLIBC__ (not __GLIBC_PREREQ) because GCC may not have
   applied its target-specific built-in defines at -include time. */
#ifndef __GLIBC__
#define __GLIBC__ 2
#define __GLIBC_MINOR__ 0
#define __GLIBC_PREREQ(maj, min) \
  ((__GLIBC__ << 16) + __GLIBC_MINOR__ >= ((maj) << 16) + (min))
#endif

/* OpenHobbyOS POSIX option flags */
#ifdef __OPENHOBBYOS__
#define _POSIX_ASYNCHRONOUS_IO            1
#define _POSIX_FSYNC                      1
#define _POSIX_MAPPED_FILES               1
#ifndef _POSIX_MONOTONIC_CLOCK
#define _POSIX_MONOTONIC_CLOCK            200112L
#endif
#ifndef _POSIX_CLOCK_SELECTION
#define _POSIX_CLOCK_SELECTION            200112L
#endif
#define _POSIX_PRIORITY_SCHEDULING        1
#define _POSIX_REALTIME_SIGNALS           1
#define _POSIX_SEMAPHORES                 1
#define _POSIX_SHARED_MEMORY_OBJECTS      1
#define _POSIX_SYNCHRONIZED_IO            1
#ifndef _POSIX_TIMERS
#define _POSIX_TIMERS                     200112L
#endif
#define _POSIX_BARRIERS                   200112L
#define _POSIX_READER_WRITER_LOCKS        200112L
#define _POSIX_SPIN_LOCKS                 200112L
#ifndef _POSIX_THREADS
#define _POSIX_THREADS                    200112L
#endif
#define _POSIX_THREAD_ATTR_STACKADDR      1
#define _POSIX_THREAD_ATTR_STACKSIZE      1
#define _POSIX_THREAD_PRIORITY_SCHEDULING 1
#define _POSIX_THREAD_PROCESS_SHARED      1
#define _POSIX_THREAD_SAFE_FUNCTIONS      1
#define _POSIX_TIMEOUTS                   200112L
#define _POSIX_PIPE_BUF                   512
#define _UNIX98_THREAD_MUTEX_ATTRIBUTES   1

/*
 * O_LARGEFILE — 0100000 on x86 Linux.  Qt references it unconditionally in
 * qplatformdefs.h even when LFS extensions are disabled for the target.
 * The value is harmless (already implicitly 0 since we don't do 64-bit file
 * ops), but the macro must exist for the build to succeed.
 */
#ifndef O_LARGEFILE
#define O_LARGEFILE 0100000
#endif

/*
 * Newlib's <time.h> has BSD/GNU tm_gmtoff and tm_zone fields behind
 * __TM_GMTOFF / __TM_ZONE preprocessor guards.  Defining these as the
 * expected names makes them appear with the standard BSD names so that
 * software (e.g. Qt) written against glibc or BSD libc can see them.
 */
#define __TM_GMTOFF tm_gmtoff
#define __TM_ZONE   tm_zone

/* libstdc++ (glibc build) expects _ISupper / _ISlower etc. from
 * <ctype.h>.  Newlib does not define these, so provide them here so
 * that the force-included features.h makes them visible before any
 * libstdc++ header that uses them (e.g. <bits/ctype_base.h>). */
#define _ISupper  0x0100
#define _ISlower  0x0200
#define _ISalpha  0x0400
#define _ISdigit  0x0800
#define _ISxdigit 0x1000
#define _ISspace  0x2000
#define _ISprint  0x4000
#define _ISgraph  0x8000
#define _ISblank  0x0001
#define _IScntrl  0x0002
#define _ISpunct  0x0004
#define _ISalnum  0x0008

#ifndef __ASSEMBLER__
#include <sys/cpuset.h>
#endif
#endif

#endif
