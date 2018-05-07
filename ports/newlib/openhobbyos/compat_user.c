#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "compat.h"

struct oh_user_record {
    const char *name;
    const char *passwd;
    uid_t uid;
    gid_t gid;
    const char *gecos;
    const char *home;
    const char *shell;
};

static const struct oh_user_record oh_users[] = {
    { "root", "root", 0, 0, "OpenHobbyOS root", "/root", "/bin/gosh" },
    { "user", "user", 1000, 1000, "OpenHobbyOS user", "/home/user", "/bin/gosh" },
};

static const struct oh_user_record *oh_find_user_by_name(const char *name) {
    for (size_t i = 0; i < sizeof(oh_users) / sizeof(oh_users[0]); ++i) {
        if (name != NULL && strcmp(oh_users[i].name, name) == 0) {
            return &oh_users[i];
        }
    }
    return NULL;
}

static const struct oh_user_record *oh_find_user_by_uid(uid_t uid) {
    for (size_t i = 0; i < sizeof(oh_users) / sizeof(oh_users[0]); ++i) {
        if (oh_users[i].uid == uid) {
            return &oh_users[i];
        }
    }
    return NULL;
}

static const struct oh_user_record *oh_find_user_by_gid(gid_t gid) {
    for (size_t i = 0; i < sizeof(oh_users) / sizeof(oh_users[0]); ++i) {
        if (oh_users[i].gid == gid) {
            return &oh_users[i];
        }
    }
    return NULL;
}

static int oh_pack_string(char **slot, char **cursor, size_t *remaining, const char *text) {
    size_t length = strlen(text) + 1;

    if (*remaining < length) {
        return ERANGE;
    }

    memcpy(*cursor, text, length);
    *slot = *cursor;
    *cursor += length;
    *remaining -= length;
    return 0;
}

static int oh_fill_passwd_from_record(const struct oh_user_record *record, struct passwd *pwd, char *buffer, size_t size, struct passwd **result) {
    char *cursor = buffer;
    size_t remaining = size;
    int status;

    if (pwd == NULL || buffer == NULL || result == NULL || record == NULL) {
        return EINVAL;
    }

    status = oh_pack_string(&pwd->pw_name, &cursor, &remaining, record->name);
    if (status == 0) {
        status = oh_pack_string(&pwd->pw_passwd, &cursor, &remaining, record->passwd);
    }
    if (status == 0) {
        status = oh_pack_string(&pwd->pw_comment, &cursor, &remaining, record->gecos);
    }
    if (status == 0) {
        status = oh_pack_string(&pwd->pw_gecos, &cursor, &remaining, record->gecos);
    }
    if (status == 0) {
        status = oh_pack_string(&pwd->pw_dir, &cursor, &remaining, record->home);
    }
    if (status == 0) {
        status = oh_pack_string(&pwd->pw_shell, &cursor, &remaining, record->shell);
    }
    if (status != 0) {
        *result = NULL;
        return status;
    }

    pwd->pw_uid = record->uid;
    pwd->pw_gid = record->gid;
    *result = pwd;
    return 0;
}

static int oh_fill_group_from_record(const struct oh_user_record *record, struct group *grp, char *buffer, size_t size, struct group **result) {
    char *cursor = buffer;
    size_t remaining = size;
    int status;

    if (grp == NULL || buffer == NULL || result == NULL || record == NULL) {
        return EINVAL;
    }

    if (remaining < sizeof(char *) * 2u) {
        *result = NULL;
        return ERANGE;
    }

    grp->gr_mem = (char **) cursor;
    cursor += sizeof(char *) * 2u;
    remaining -= sizeof(char *) * 2u;

    status = oh_pack_string(&grp->gr_name, &cursor, &remaining, record->name);
    if (status == 0) {
        status = oh_pack_string(&grp->gr_passwd, &cursor, &remaining, record->passwd);
    }
    if (status == 0) {
        status = oh_pack_string(&grp->gr_mem[0], &cursor, &remaining, record->name);
    }
    if (status != 0) {
        *result = NULL;
        return status;
    }

    grp->gr_mem[1] = NULL;
    grp->gr_gid = record->gid;
    *result = grp;
    return 0;
}

int uname(struct utsname *name) {
    struct linux_utsname native_name;
    int result;

    if (name == NULL) {
        errno = EFAULT;
        return -1;
    }

    result = oh_uname_raw(&native_name);
    if (result < 0) {
        errno = -result;
        return -1;
    }

    memcpy(name->sysname, native_name.sysname, sizeof(name->sysname));
    memcpy(name->nodename, native_name.nodename, sizeof(name->nodename));
    memcpy(name->release, native_name.release, sizeof(name->release));
    memcpy(name->version, native_name.version, sizeof(name->version));
    memcpy(name->machine, native_name.machine, sizeof(name->machine));
    return 0;
}

int gethostname(char *name, size_t len) {
    struct utsname uts;
    size_t needed;

    if (name == NULL || len == 0) {
        errno = EINVAL;
        return -1;
    }
    if (uname(&uts) != 0) {
        return -1;
    }

    needed = strlen(uts.nodename) + 1;
    if (needed > len) {
        errno = ENAMETOOLONG;
        return -1;
    }

    memcpy(name, uts.nodename, needed);
    return 0;
}

pid_t getpid(void) {
    return (pid_t) oh_getpid_raw();
}

pid_t getppid(void) {
    return 1;
}

uid_t getuid(void) {
    return (uid_t) oh_getuid_raw();
}

gid_t getgid(void) {
    return (gid_t) oh_getgid_raw();
}

uid_t geteuid(void) {
    return (uid_t) oh_geteuid_raw();
}

gid_t getegid(void) {
    return (gid_t) oh_getegid_raw();
}

int setuid(uid_t uid) {
    return oh_check_result(oh_setuid_raw((unsigned int)uid));
}

int seteuid(uid_t uid) {
    return oh_check_result(oh_seteuid_raw((unsigned int)uid));
}

int setgid(gid_t gid) {
    return oh_check_result(oh_setgid_raw((unsigned int)gid));
}

int setegid(gid_t gid) {
    return oh_check_result(oh_setegid_raw((unsigned int)gid));
}

pid_t getpgrp(void) {
    return getpid();
}

int setpgid(pid_t pid, pid_t pgid) {
    pid_t self = getpid();

    if (pid != 0 && pid != self) {
        errno = ESRCH;
        return -1;
    }
    if (pgid != 0 && pgid != self) {
        errno = EPERM;
        return -1;
    }

    return 0;
}

char *getlogin(void) {
    const struct oh_user_record *record = oh_find_user_by_uid((uid_t)oh_getuid_raw());
    const struct oh_user_record *fallback = oh_find_user_by_uid(0);
    return (char *)(record ? record->name : (fallback ? fallback->name : "root"));
}

int getlogin_r(char *name, size_t namesize) {
    const struct oh_user_record *record = oh_find_user_by_uid((uid_t)oh_getuid_raw());
    const char *user_name = record ? record->name : "root";
    size_t needed = strlen(user_name) + 1;

    if (name == NULL || namesize < needed) {
        return ERANGE;
    }

    memcpy(name, user_name, needed);
    return 0;
}

struct passwd *getpwuid(uid_t uid) {
    const struct oh_user_record *record = oh_find_user_by_uid(uid);
    struct passwd *result = NULL;

    if (record == NULL) {
        errno = ENOENT;
        return NULL;
    }
    static struct passwd oh_passwd;
    static char buffer[256];
    if (oh_fill_passwd_from_record(record, &oh_passwd, buffer, sizeof(buffer), &result) != 0 || result == NULL) {
        errno = ERANGE;
        return NULL;
    }
    return &oh_passwd;
}

struct passwd *getpwnam(const char *name) {
    const struct oh_user_record *record = oh_find_user_by_name(name);

    if (record == NULL) {
        errno = ENOENT;
        return NULL;
    }
    static struct passwd oh_passwd;
    static char buffer[256];
    struct passwd *result = NULL;
    if (oh_fill_passwd_from_record(record, &oh_passwd, buffer, sizeof(buffer), &result) != 0 || result == NULL) {
        errno = ERANGE;
        return NULL;
    }
    return &oh_passwd;
}

int getpwuid_r(uid_t uid, struct passwd *pwd, char *buffer, size_t size, struct passwd **result) {
    const struct oh_user_record *record = oh_find_user_by_uid(uid);
    if (record == NULL) {
        if (result) {
            *result = NULL;
        }
        return ENOENT;
    }
    return oh_fill_passwd_from_record(record, pwd, buffer, size, result);
}

int getpwnam_r(const char *name, struct passwd *pwd, char *buffer, size_t size, struct passwd **result) {
    const struct oh_user_record *record = oh_find_user_by_name(name);
    if (record == NULL) {
        if (result) {
            *result = NULL;
        }
        return ENOENT;
    }
    return oh_fill_passwd_from_record(record, pwd, buffer, size, result);
}

struct group *getgrgid(gid_t gid) {
    const struct oh_user_record *record = oh_find_user_by_gid(gid);
    if (record == NULL) {
        errno = ENOENT;
        return NULL;
    }
    static struct group oh_group;
    static char buffer[128];
    struct group *result = NULL;
    if (oh_fill_group_from_record(record, &oh_group, buffer, sizeof(buffer), &result) != 0 || result == NULL) {
        errno = ERANGE;
        return NULL;
    }
    return &oh_group;
}

struct group *getgrnam(const char *name) {
    const struct oh_user_record *record = oh_find_user_by_name(name);
    if (record == NULL) {
        errno = ENOENT;
        return NULL;
    }
    static struct group oh_group;
    static char buffer[128];
    struct group *result = NULL;
    if (oh_fill_group_from_record(record, &oh_group, buffer, sizeof(buffer), &result) != 0 || result == NULL) {
        errno = ERANGE;
        return NULL;
    }
    return &oh_group;
}

int getgrgid_r(gid_t gid, struct group *grp, char *buffer, size_t size, struct group **result) {
    const struct oh_user_record *record = oh_find_user_by_gid(gid);
    if (record == NULL) {
        if (result) {
            *result = NULL;
        }
        return ENOENT;
    }
    return oh_fill_group_from_record(record, grp, buffer, size, result);
}

int getgrnam_r(const char *name, struct group *grp, char *buffer, size_t size, struct group **result) {
    const struct oh_user_record *record = oh_find_user_by_name(name);
    if (record == NULL) {
        if (result) {
            *result = NULL;
        }
        return ENOENT;
    }
    return oh_fill_group_from_record(record, grp, buffer, size, result);
}

long sysconf(int name) {
    switch (name) {
#ifdef _SC_ARG_MAX
        case _SC_ARG_MAX:
            return 4096;
#endif
#ifdef _SC_CLK_TCK
        case _SC_CLK_TCK:
            return 100;
#endif
#ifdef _SC_OPEN_MAX
        case _SC_OPEN_MAX:
            return 32;
#endif
#if defined(_SC_PAGESIZE)
        case _SC_PAGESIZE:
#endif
#if defined(_SC_PAGE_SIZE) && (!defined(_SC_PAGESIZE) || _SC_PAGE_SIZE != _SC_PAGESIZE)
        case _SC_PAGE_SIZE:
#endif
            return 4096;
#ifdef _SC_NPROCESSORS_CONF
        case _SC_NPROCESSORS_CONF:
            return 1;
#endif
#ifdef _SC_NPROCESSORS_ONLN
        case _SC_NPROCESSORS_ONLN:
            return 1;
#endif
#ifdef _SC_PHYS_PAGES
        case _SC_PHYS_PAGES:
            return 4096;
#endif
#ifdef _SC_AVPHYS_PAGES
        case _SC_AVPHYS_PAGES:
            return 2048;
#endif
#ifdef _SC_TTY_NAME_MAX
        case _SC_TTY_NAME_MAX:
            return 16;
#endif
#ifdef _SC_GETPW_R_SIZE_MAX
        case _SC_GETPW_R_SIZE_MAX:
            return 128;
#endif
#ifdef _SC_LOGIN_NAME_MAX
        case _SC_LOGIN_NAME_MAX:
            return 16;
#endif
#ifdef _SC_HOST_NAME_MAX
        case _SC_HOST_NAME_MAX:
            return 64;
#endif
        default:
            errno = EINVAL;
            return -1;
    }
}

int getpagesize(void) {
    return 4096;
}

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
    static sigset_t current_mask;

    if (oldset) {
        *oldset = current_mask;
    }

    if (set == NULL) {
        return 0;
    }

    switch (how) {
        case SIG_BLOCK:
            current_mask |= *set;
            return 0;
        case SIG_UNBLOCK:
            current_mask &= (sigset_t) ~(*set);
            return 0;
        case SIG_SETMASK:
            current_mask = *set;
            return 0;
        default:
            errno = EINVAL;
            return -1;
    }
}
