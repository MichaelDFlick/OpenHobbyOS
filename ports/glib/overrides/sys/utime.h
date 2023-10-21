#ifndef _OVERRIDE_SYS_UTIME_H
#define _OVERRIDE_SYS_UTIME_H

#include_next <sys/utime.h>

int utime(const char *path, const struct utimbuf *times);

#endif
