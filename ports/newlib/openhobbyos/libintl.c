#include <libintl.h>

char *dcgettext(const char *domainname, const char *msgid, int category) {
    (void)domainname;
    (void)category;
    return (char *)msgid;
}

char *textdomain(const char *domainname) {
    return (char *)domainname;
}

char *bindtextdomain(const char *domainname, const char *dirname) {
    (void)domainname;
    (void)dirname;
    return (char *)dirname;
}
