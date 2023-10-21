#ifndef _RESOLV_H_
#define _RESOLV_H_

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>

#define RESOLVSZ 4096

/* h_errno and resolver error codes */
extern int h_errno;

#ifndef HOST_NOT_FOUND
#define HOST_NOT_FOUND 1
#endif
#ifndef TRY_AGAIN
#define TRY_AGAIN 2
#endif
#ifndef NO_RECOVERY
#define NO_RECOVERY 3
#endif
#ifndef NO_DATA
#define NO_DATA 4
#endif
#ifndef NETDB_INTERNAL
#define NETDB_INTERNAL -1
#endif

/* DNS HEADER structure */
typedef struct {
    unsigned id :16;
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    unsigned qr: 1;
    unsigned opcode: 4;
    unsigned aa: 1;
    unsigned tc: 1;
    unsigned rd: 1;
    unsigned ra: 1;
    unsigned unused :1;
    unsigned ad: 1;
    unsigned cd: 1;
    unsigned rcode :4;
#else
    unsigned rd :1;
    unsigned tc :1;
    unsigned aa :1;
    unsigned opcode :4;
    unsigned qr :1;
    unsigned rcode :4;
    unsigned cd: 1;
    unsigned ad: 1;
    unsigned unused :1;
    unsigned ra :1;
#endif
    unsigned qdcount :16;
    unsigned ancount :16;
    unsigned nscount :16;
    unsigned arcount :16;
} HEADER;

/* NS_GET16 / NS_GET32 from arpa/nameser.h should provide these */
#ifndef GETSHORT
#define GETSHORT NS_GET16
#endif
#ifndef GETLONG
#define GETLONG NS_GET32
#endif

#ifndef C_IN
#define C_IN ns_c_in
#endif

struct __res_state {
    int retrans;
    int retry;
    unsigned long options;
    int nscount;
    struct sockaddr_in nsaddr_list[3];
    unsigned short id;
    char defdname[256];
};

#define RES_INIT 0x0001

__BEGIN_DECLS

int res_init(void);
int res_query(const char *dname, int class, int type, unsigned char *answer, int anslen);
int res_search(const char *dname, int class, int type, unsigned char *answer, int anslen);
int res_mkquery(int op, const char *dname, int class, int type, const unsigned char *data, int datalen, const unsigned char *newrr, unsigned char *buf, int buflen);
int res_send(const unsigned char *msg, int msglen, unsigned char *answer, int anslen);
void res_close(void);
int dn_expand(const unsigned char *msg, const unsigned char *eom, const unsigned char *src, char *dst, int dstsiz);

__END_DECLS

#endif
