#ifndef OPENHOBBYOS_SYS_UTSNAME_H
#define OPENHOBBYOS_SYS_UTSNAME_H

#include <sys/cdefs.h>

__BEGIN_DECLS

struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
};

int uname(struct utsname *name);

__END_DECLS

#endif
