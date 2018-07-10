#ifndef _LIBINTL_H
#define _LIBINTL_H

#define __USE_GNU_GETTEXT 1

#define _(msgid) ((const char *)(msgid))
#define N_(msgid) (msgid)

#ifdef __cplusplus
extern "C" {
#endif

char *dcgettext(const char *domainname, const char *msgid, int category);
char *textdomain(const char *domainname);
char *bindtextdomain(const char *domainname, const char *dirname);

#ifdef __cplusplus
}
#endif

#endif
