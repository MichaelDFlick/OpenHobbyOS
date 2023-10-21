#ifndef _ARPA_NAMESER_H_
#define _ARPA_NAMESER_H_

#include <sys/types.h>
#include <sys/cdefs.h>

#define NS_PACKETSZ 512
#define NS_MAXDNAME 1025
#define NS_MAXMSG 65535
#define NS_MAXCDNAME 255
#define NS_MAXLABEL 63
#define NS_HFIXEDSZ 12
#define NS_QFIXEDSZ 4
#define NS_RRFIXEDSZ 10
#define NS_INT32SZ 4
#define NS_INT16SZ 2
#define NS_INT8SZ  1
#define NS_INADDRSZ 4
#define NS_IN6ADDRSZ 16
#define NS_CMPRSFLGS 0xc0

typedef enum __ns_class {
    ns_c_invalid = 0,
    ns_c_in = 1,
    ns_c_2 = 2,
    ns_c_chaos = 3,
    ns_c_hs = 4,
    ns_c_none = 254,
    ns_c_any = 255,
    ns_c_max = 65536
} ns_class;

#define C_IN ns_c_in

typedef enum __ns_type {
    ns_t_invalid = 0,
    ns_t_a = 1,
    ns_t_ns = 2,
    ns_t_md = 3,
    ns_t_mf = 4,
    ns_t_cname = 5,
    ns_t_soa = 6,
    ns_t_mb = 7,
    ns_t_mg = 8,
    ns_t_mr = 9,
    ns_t_null = 10,
    ns_t_wks = 11,
    ns_t_ptr = 12,
    ns_t_hinfo = 13,
    ns_t_minfo = 14,
    ns_t_mx = 15,
    ns_t_txt = 16,
    ns_t_rp = 17,
    ns_t_afsdb = 18,
    ns_t_x25 = 19,
    ns_t_isdn = 20,
    ns_t_rt = 21,
    ns_t_nsap = 22,
    ns_t_nsap_ptr = 23,
    ns_t_sig = 24,
    ns_t_key = 25,
    ns_t_px = 26,
    ns_t_gpos = 27,
    ns_t_aaaa = 28,
    ns_t_loc = 29,
    ns_t_nxt = 30,
    ns_t_eid = 31,
    ns_t_nimloc = 32,
    ns_t_srv = 33,
    ns_t_atma = 34,
    ns_t_naptr = 35,
    ns_t_kx = 36,
    ns_t_cert = 37,
    ns_t_a6 = 38,
    ns_t_dname = 39,
    ns_t_sink = 40,
    ns_t_opt = 41,
    ns_t_tsig = 250,
    ns_t_ixfr = 251,
    ns_t_axfr = 252,
    ns_t_mailb = 253,
    ns_t_maila = 254,
    ns_t_any = 255,
    ns_t_zxfr = 256,
    ns_t_max = 65536
} ns_type;

/* NS_GET16 / NS_GET32 macros for DNS response parsing */
#ifndef NS_GET16
#define NS_GET16(s, cp) do { \
    (s) = ((unsigned char)(cp)[0] << 8) | ((unsigned char)(cp)[1]); \
    (cp) += 2; \
} while (0)
#endif

#ifndef NS_GET32
#define NS_GET32(l, cp) do { \
    (l) = ((unsigned long)((unsigned char)(cp)[0]) << 24) | \
          ((unsigned long)((unsigned char)(cp)[1]) << 16) | \
          ((unsigned long)((unsigned char)(cp)[2]) << 8) | \
          ((unsigned long)((unsigned char)(cp)[3])); \
    (cp) += 4; \
} while (0)
#endif

/* Old-style BIND constants (aliases for ns_type enum) */
#ifndef T_A
#define T_A ns_t_a
#endif
#ifndef T_NS
#define T_NS ns_t_ns
#endif
#ifndef T_MD
#define T_MD ns_t_md
#endif
#ifndef T_MF
#define T_MF ns_t_mf
#endif
#ifndef T_CNAME
#define T_CNAME ns_t_cname
#endif
#ifndef T_SOA
#define T_SOA ns_t_soa
#endif
#ifndef T_MB
#define T_MB ns_t_mb
#endif
#ifndef T_MG
#define T_MG ns_t_mg
#endif
#ifndef T_MR
#define T_MR ns_t_mr
#endif
#ifndef T_NULL
#define T_NULL ns_t_null
#endif
#ifndef T_WKS
#define T_WKS ns_t_wks
#endif
#ifndef T_PTR
#define T_PTR ns_t_ptr
#endif
#ifndef T_HINFO
#define T_HINFO ns_t_hinfo
#endif
#ifndef T_MINFO
#define T_MINFO ns_t_minfo
#endif
#ifndef T_MX
#define T_MX ns_t_mx
#endif
#ifndef T_TXT
#define T_TXT ns_t_txt
#endif
#ifndef T_RP
#define T_RP ns_t_rp
#endif
#ifndef T_AFSDB
#define T_AFSDB ns_t_afsdb
#endif
#ifndef T_X25
#define T_X25 ns_t_x25
#endif
#ifndef T_ISDN
#define T_ISDN ns_t_isdn
#endif
#ifndef T_RT
#define T_RT ns_t_rt
#endif
#ifndef T_NSAP
#define T_NSAP ns_t_nsap
#endif
#ifndef T_NSAP_PTR
#define T_NSAP_PTR ns_t_nsap_ptr
#endif
#ifndef T_SIG
#define T_SIG ns_t_sig
#endif
#ifndef T_KEY
#define T_KEY ns_t_key
#endif
#ifndef T_PX
#define T_PX ns_t_px
#endif
#ifndef T_GPOS
#define T_GPOS ns_t_gpos
#endif
#ifndef T_AAAA
#define T_AAAA ns_t_aaaa
#endif
#ifndef T_LOC
#define T_LOC ns_t_loc
#endif
#ifndef T_NXT
#define T_NXT ns_t_nxt
#endif
#ifndef T_EID
#define T_EID ns_t_eid
#endif
#ifndef T_NIMLOC
#define T_NIMLOC ns_t_nimloc
#endif
#ifndef T_SRV
#define T_SRV ns_t_srv
#endif
#ifndef T_ATMA
#define T_ATMA ns_t_atma
#endif
#ifndef T_NAPTR
#define T_NAPTR ns_t_naptr
#endif
#ifndef T_KX
#define T_KX ns_t_kx
#endif
#ifndef T_CERT
#define T_CERT ns_t_cert
#endif
#ifndef T_A6
#define T_A6 ns_t_a6
#endif
#ifndef T_DNAME
#define T_DNAME ns_t_dname
#endif
#ifndef T_SINK
#define T_SINK ns_t_sink
#endif
#ifndef T_OPT
#define T_OPT ns_t_opt
#endif
#ifndef T_TSIG
#define T_TSIG ns_t_tsig
#endif
#ifndef T_IXFR
#define T_IXFR ns_t_ixfr
#endif
#ifndef T_AXFR
#define T_AXFR ns_t_axfr
#endif
#ifndef T_MAILB
#define T_MAILB ns_t_mailb
#endif
#ifndef T_MAILA
#define T_MAILA ns_t_maila
#endif
#ifndef T_ANY
#define T_ANY ns_t_any
#endif

__BEGIN_DECLS

int res_init(void);
int res_query(const char *dname, int class, int type, unsigned char *answer, int anslen);
int res_search(const char *dname, int class, int type, unsigned char *answer, int anslen);
int res_mkquery(int op, const char *dname, int class, int type, const unsigned char *data, int datalen, const unsigned char *newrr, unsigned char *buf, int buflen);
int res_send(const unsigned char *msg, int msglen, unsigned char *answer, int anslen);
void res_close(void);

__END_DECLS

#endif
