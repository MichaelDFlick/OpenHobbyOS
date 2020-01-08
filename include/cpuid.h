#ifndef OHOS_CPUID_H
#define OHOS_CPUID_H

#include "types.h"

#define CPUID_FEATURE_ECX_SSE3         (1u << 0)
#define CPUID_FEATURE_ECX_PCLMULQDQ    (1u << 1)
#define CPUID_FEATURE_ECX_DTES64       (1u << 2)
#define CPUID_FEATURE_ECX_MONITOR      (1u << 3)
#define CPUID_FEATURE_ECX_DSCPL        (1u << 4)
#define CPUID_FEATURE_ECX_VMX          (1u << 5)
#define CPUID_FEATURE_ECX_SMX          (1u << 6)
#define CPUID_FEATURE_ECX_EST          (1u << 7)
#define CPUID_FEATURE_ECX_TM2          (1u << 8)
#define CPUID_FEATURE_ECX_SSSE3        (1u << 9)
#define CPUID_FEATURE_ECX_CNXTID       (1u << 10)
#define CPUID_FEATURE_ECX_FMA          (1u << 12)
#define CPUID_FEATURE_ECX_CX16         (1u << 13)
#define CPUID_FEATURE_ECX_XTPR         (1u << 14)
#define CPUID_FEATURE_ECX_PDCM         (1u << 15)
#define CPUID_FEATURE_ECX_PCID         (1u << 17)
#define CPUID_FEATURE_ECX_DCA          (1u << 18)
#define CPUID_FEATURE_ECX_SSE4_1       (1u << 19)
#define CPUID_FEATURE_ECX_SSE4_2       (1u << 20)
#define CPUID_FEATURE_ECX_X2APIC       (1u << 21)
#define CPUID_FEATURE_ECX_MOVBE        (1u << 22)
#define CPUID_FEATURE_ECX_POPCNT       (1u << 23)
#define CPUID_FEATURE_ECX_TSCDEADLINE  (1u << 24)
#define CPUID_FEATURE_ECX_AES          (1u << 25)
#define CPUID_FEATURE_ECX_XSAVE        (1u << 26)
#define CPUID_FEATURE_ECX_OSXSAVE      (1u << 27)
#define CPUID_FEATURE_ECX_AVX          (1u << 28)
#define CPUID_FEATURE_ECX_F16C         (1u << 29)
#define CPUID_FEATURE_ECX_RDRAND       (1u << 30)
#define CPUID_FEATURE_ECX_HYPERVISOR   (1u << 31)

#define CPUID_FEATURE_EDX_FPU          (1u << 0)
#define CPUID_FEATURE_EDX_VME          (1u << 1)
#define CPUID_FEATURE_EDX_DE           (1u << 2)
#define CPUID_FEATURE_EDX_PSE          (1u << 3)
#define CPUID_FEATURE_EDX_TSC          (1u << 4)
#define CPUID_FEATURE_EDX_MSR          (1u << 5)
#define CPUID_FEATURE_EDX_PAE          (1u << 6)
#define CPUID_FEATURE_EDX_MCE          (1u << 7)
#define CPUID_FEATURE_EDX_CX8          (1u << 8)
#define CPUID_FEATURE_EDX_APIC         (1u << 9)
#define CPUID_FEATURE_EDX_SEP          (1u << 11)
#define CPUID_FEATURE_EDX_MTRR         (1u << 12)
#define CPUID_FEATURE_EDX_PGE          (1u << 13)
#define CPUID_FEATURE_EDX_MCA          (1u << 14)
#define CPUID_FEATURE_EDX_CMOV         (1u << 15)
#define CPUID_FEATURE_EDX_PAT          (1u << 16)
#define CPUID_FEATURE_EDX_PSE36        (1u << 17)
#define CPUID_FEATURE_EDX_PSN          (1u << 18)
#define CPUID_FEATURE_EDX_CLFLUSH      (1u << 19)
#define CPUID_FEATURE_EDX_DTES         (1u << 21)
#define CPUID_FEATURE_EDX_ACPI         (1u << 22)
#define CPUID_FEATURE_EDX_MMX          (1u << 23)
#define CPUID_FEATURE_EDX_FXSR         (1u << 24)
#define CPUID_FEATURE_EDX_SSE          (1u << 25)
#define CPUID_FEATURE_EDX_SSE2         (1u << 26)
#define CPUID_FEATURE_EDX_SS           (1u << 27)
#define CPUID_FEATURE_EDX_HTT          (1u << 28)
#define CPUID_FEATURE_EDX_TM1          (1u << 29)
#define CPUID_FEATURE_EDX_IA64         (1u << 30)
#define CPUID_FEATURE_EDX_PBE          (1u << 31)

typedef struct {
    char vendor[16];
    char brand[65];
    u32 family;
    u32 model;
    u32 stepping;
    u32 type;
    u32 ext_family;
    u32 ext_model;
    u32 cache_size_kb;
    u32 features_ecx;
    u32 features_edx;
    u32 max_leaf;
    u32 max_ext_leaf;
    bool brand_string_valid;
} cpu_info_t;

static inline void cpuid(u32 leaf, u32 *eax, u32 *ebx, u32 *ecx, u32 *edx) {
    __asm__ volatile(
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf)
    );
}

void cpu_detect(void);
const cpu_info_t *cpu_get_info(void);

#endif
