#ifndef _SYS_CPUSET_H_
#define _SYS_CPUSET_H_

#define CPU_SETSIZE 1024
#define __CPU_BITTYPE unsigned long
#define __CPU_BITS   (8 * sizeof(__CPU_BITTYPE))
#define __CPU_ELT(n) ((n) / __CPU_BITS)
#define __CPU_MASK(n) ((__CPU_BITTYPE)1 << ((n) % __CPU_BITS))

typedef struct {
    __CPU_BITTYPE __bits[CPU_SETSIZE / __CPU_BITS];
} cpu_set_t;

#define CPU_ZERO(cpusetp) \
    do { \
        unsigned int __cpu_i; \
        for (__cpu_i = 0; __cpu_i < sizeof(cpu_set_t) / sizeof(__CPU_BITTYPE); __cpu_i++) \
            (cpusetp)->__bits[__cpu_i] = 0; \
    } while (0)

#define CPU_SET(n, cpusetp) \
    ((cpusetp)->__bits[__CPU_ELT(n)] |= __CPU_MASK(n))

#define CPU_CLR(n, cpusetp) \
    ((cpusetp)->__bits[__CPU_ELT(n)] &= ~__CPU_MASK(n))

#define CPU_ISSET(n, cpusetp) \
    (((cpusetp)->__bits[__CPU_ELT(n)] & __CPU_MASK(n)) != 0)

#define CPU_COUNT_S(setsize, cpusetp) \
    ({ \
        size_t __cpuset_n = (setsize) / sizeof(__CPU_BITTYPE); \
        size_t __cpuset_i; \
        int __cpuset_cnt = 0; \
        for (__cpuset_i = 0; __cpuset_i < __cpuset_n; __cpuset_i++) \
            __cpuset_cnt += __builtin_popcountl((cpusetp)->__bits[__cpuset_i]); \
        __cpuset_cnt; \
    })

#define CPU_COUNT(cpusetp) CPU_COUNT_S(sizeof(cpu_set_t), cpusetp)

#define CPU_AND(dst, src1, src2) \
    do { \
        unsigned int __cpu_i; \
        for (__cpu_i = 0; __cpu_i < sizeof(cpu_set_t) / sizeof(__CPU_BITTYPE); __cpu_i++) \
            (dst)->__bits[__cpu_i] = (src1)->__bits[__cpu_i] & (src2)->__bits[__cpu_i]; \
    } while (0)

#define CPU_OR(dst, src1, src2) \
    do { \
        unsigned int __cpu_i; \
        for (__cpu_i = 0; __cpu_i < sizeof(cpu_set_t) / sizeof(__CPU_BITTYPE); __cpu_i++) \
            (dst)->__bits[__cpu_i] = (src1)->__bits[__cpu_i] | (src2)->__bits[__cpu_i]; \
    } while (0)

#define CPU_XOR(dst, src1, src2) \
    do { \
        unsigned int __cpu_i; \
        for (__cpu_i = 0; __cpu_i < sizeof(cpu_set_t) / sizeof(__CPU_BITTYPE); __cpu_i++) \
            (dst)->__bits[__cpu_i] = (src1)->__bits[__cpu_i] ^ (src2)->__bits[__cpu_i]; \
    } while (0)

#define CPU_EQUAL(cpusetp1, cpusetp2) \
    ({ \
        unsigned int __cpu_i; \
        int __cpu_eq = 1; \
        for (__cpu_i = 0; __cpu_i < sizeof(cpu_set_t) / sizeof(__CPU_BITTYPE); __cpu_i++) \
            if ((cpusetp1)->__bits[__cpu_i] != (cpusetp2)->__bits[__cpu_i]) { \
                __cpu_eq = 0; \
                break; \
            } \
        __cpu_eq; \
    })

#define CPU_ALLOC_SIZE(count) \
    ((((count) + __CPU_BITS - 1) / __CPU_BITS) * sizeof(__CPU_BITTYPE))

#endif
