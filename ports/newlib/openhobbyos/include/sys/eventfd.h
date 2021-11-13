#ifndef _SYS_EVENTFD_H_
#define _SYS_EVENTFD_H_

#include <sys/types.h>
#include <fcntl.h>

#define EFD_CLOEXEC 0x80000u
#define EFD_NONBLOCK 04000
#define EFD_SEMAPHORE 1

int eventfd(unsigned int initval, int flags);

#endif
