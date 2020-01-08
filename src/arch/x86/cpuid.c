#include "cpuid.h"
#include "console.h"
#include "string.h"

static cpu_info_t g_cpu_info;
static bool g_cpu_detected = false;

void cpu_detect(void) {
    u32 eax, ebx, ecx, edx;

    if (g_cpu_detected) return;

    memset(&g_cpu_info, 0, sizeof(g_cpu_info));

    cpuid(0, &eax, &ebx, &ecx, &edx);
    g_cpu_info.max_leaf = eax;
    memcpy(g_cpu_info.vendor + 0, &ebx, 4);
    memcpy(g_cpu_info.vendor + 4, &edx, 4);
    memcpy(g_cpu_info.vendor + 8, &ecx, 4);
    g_cpu_info.vendor[12] = '\0';

    if (g_cpu_info.max_leaf >= 1) {
        cpuid(1, &eax, &ebx, &ecx, &edx);
        g_cpu_info.stepping    = eax & 0xF;
        g_cpu_info.model       = (eax >> 4) & 0xF;
        g_cpu_info.family      = (eax >> 8) & 0xF;
        g_cpu_info.type        = (eax >> 12) & 0x3;
        g_cpu_info.ext_model   = (eax >> 16) & 0xF;
        g_cpu_info.ext_family  = (eax >> 20) & 0xFF;
        g_cpu_info.features_ecx = ecx;
        g_cpu_info.features_edx = edx;

        if (g_cpu_info.family == 0xF) {
            g_cpu_info.family += g_cpu_info.ext_family;
        }
        if (g_cpu_info.family == 0x6 || g_cpu_info.family == 0xF) {
            g_cpu_info.model = (g_cpu_info.ext_model << 4) | g_cpu_info.model;
        }

    }

    cpuid(0x80000000u, &eax, &ebx, &ecx, &edx);
    g_cpu_info.max_ext_leaf = eax;

    if (g_cpu_info.max_ext_leaf >= 0x80000004u) {
        u32 brand[12];
        cpuid(0x80000002u, brand + 0, brand + 1, brand + 2, brand + 3);
        cpuid(0x80000003u, brand + 4, brand + 5, brand + 6, brand + 7);
        cpuid(0x80000004u, brand + 8, brand + 9, brand + 10, brand + 11);
        memcpy(g_cpu_info.brand, brand, 48);
        g_cpu_info.brand[48] = '\0';
        /* strip trailing spaces */
        {
            int len = (int)strlen(g_cpu_info.brand);
            while (len > 0 && g_cpu_info.brand[len - 1] == ' ') {
                g_cpu_info.brand[--len] = '\0';
            }
        }
        g_cpu_info.brand_string_valid = (g_cpu_info.brand[0] != '\0');
    }

    if (g_cpu_info.max_ext_leaf >= 0x80000006u) {
        cpuid(0x80000006u, &eax, &ebx, &ecx, &edx);
        g_cpu_info.cache_size_kb = (ecx >> 16) & 0xFFFF;
    }

    g_cpu_detected = true;

    console_printf("[cpu] vendor=%s family=%u model=%u stepping=%u\n",
                   g_cpu_info.vendor, g_cpu_info.family,
                   g_cpu_info.model, g_cpu_info.stepping);
    if (g_cpu_info.brand_string_valid) {
        console_printf("[cpu] %s\n", g_cpu_info.brand);
    }
    if (g_cpu_info.cache_size_kb) {
        console_printf("[cpu] L2 cache: %u KB\n", g_cpu_info.cache_size_kb);
    }
}

const cpu_info_t *cpu_get_info(void) {
    return g_cpu_detected ? &g_cpu_info : NULL;
}
