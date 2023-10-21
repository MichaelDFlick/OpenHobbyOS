#include <resolv.h>
#include <string.h>

int res_init(void) { return 0; }
int res_query(const char *dname, int class, int type, unsigned char *answer, int anslen) { (void)dname; (void)class; (void)type; (void)answer; (void)anslen; return -1; }
int res_search(const char *dname, int class, int type, unsigned char *answer, int anslen) { (void)dname; (void)class; (void)type; (void)answer; (void)anslen; return -1; }
int res_mkquery(int op, const char *dname, int class, int type, const unsigned char *data, int datalen, const unsigned char *newrr, unsigned char *buf, int buflen) { (void)op; (void)dname; (void)class; (void)type; (void)data; (void)datalen; (void)newrr; (void)buf; (void)buflen; return -1; }
int res_send(const unsigned char *msg, int msglen, unsigned char *answer, int anslen) { (void)msg; (void)msglen; (void)answer; (void)anslen; return -1; }
void res_close(void) {}
int dn_expand(const unsigned char *msg, const unsigned char *eom, const unsigned char *src, char *dst, int dstsiz) { (void)msg; (void)eom; (void)src; (void)dst; (void)dstsiz; return -1; }
