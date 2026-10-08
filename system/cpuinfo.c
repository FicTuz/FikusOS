/* ============================================================
 * cpuinfo.c — CPU info commands
 *   cpuid()  — vendor, brand, exact model (FMS + stepping)
 *   fcpuid() — everything
 *   hcpuid() — hardware: cores, threads, topology, caches, freq
 * ============================================================ */

#include <stdint.h>
#include <stddef.h>
#include "../video/video.h"
#include "cpuinfo.h"

/* ---- VGA colors ---- */
#define C_WHITE   0x0F
#define C_GREEN   0x0A
#define C_YELLOW  0x0E
#define C_CYAN    0x0B
#define C_GRAY    0x07
#define C_RED     0x0C

/* ============================================================
 * String buffer (no libc)
 * ============================================================ */

static char  line[160];
static int   line_len;

static void buf_reset(void) { line_len = 0; line[0] = '\0'; }

static void buf_ch(char c)
{
    if (line_len < (int)sizeof(line) - 2) {
        line[line_len++] = c;
        line[line_len]   = '\0';
    }
}

static void buf_str(const char *s) { while (*s) buf_ch(*s++); }

static void buf_u32(uint32_t v)
{
    char t[12]; int i = 0;
    if (v == 0) { buf_ch('0'); return; }
    while (v) { t[i++] = '0' + (v % 10); v /= 10; }
    while (i--) buf_ch(t[i]);
}

static void buf_hex2(uint32_t v)
{
    const char h[] = "0123456789ABCDEF";
    buf_ch(h[(v >> 4) & 0xF]);
    buf_ch(h[ v       & 0xF]);
}

static void buf_pad(int width) { while (line_len < width) buf_ch(' '); }

/* Завершает буфер переводом строки и печатает */
static void buf_flush(uint8_t color)
{
    buf_ch('\n');
    print(line, color);
}

/* ============================================================
 * helpers
 * ============================================================ */

static void field(const char *label, const char *value, uint8_t color)
{
    buf_reset();
    buf_str(label);
    buf_pad(22);
    buf_str(": ");
    buf_str(value);
    buf_flush(color);
}

static void field_u(const char *label, uint32_t v, const char *suffix, uint8_t color)
{
    buf_reset();
    buf_str(label);
    buf_pad(22);
    buf_str(": ");
    buf_u32(v);
    if (suffix) buf_str(suffix);
    buf_flush(color);
}

static void header(const char *title)
{
    print("==========================================\n", C_CYAN);

    buf_reset();
    int len = 0; const char *p = title;
    while (*p++) len++;
    int pad = (42 - len) / 2;
    if (pad < 0) pad = 0;
    while (pad--) buf_ch(' ');
    buf_str(title);
    buf_flush(C_CYAN);

    print("==========================================\n", C_CYAN);
}

static void blank(void) { print("\n", C_WHITE); }

/* ============================================================
 * CPUID primitives
 * ============================================================ */

static inline void cpuid_raw(uint32_t leaf, uint32_t subleaf,
                             uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d)
{
#if defined(__x86_64__) || defined(__i386__)
    __asm__ __volatile__("cpuid"
        : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
        : "a"(leaf), "c"(subleaf));
#else
    *a = *b = *c = *d = 0;
#endif
}

static void get_vendor(char out[13])
{
    uint32_t a, b, c, d;
    cpuid_raw(0, 0, &a, &b, &c, &d);
    out[0]  = (char)( b        & 0xFF);
    out[1]  = (char)((b >>  8) & 0xFF);
    out[2]  = (char)((b >> 16) & 0xFF);
    out[3]  = (char)((b >> 24) & 0xFF);
    out[4]  = (char)( d        & 0xFF);
    out[5]  = (char)((d >>  8) & 0xFF);
    out[6]  = (char)((d >> 16) & 0xFF);
    out[7]  = (char)((d >> 24) & 0xFF);
    out[8]  = (char)( c        & 0xFF);
    out[9]  = (char)((c >>  8) & 0xFF);
    out[10] = (char)((c >> 16) & 0xFF);
    out[11] = (char)((c >> 24) & 0xFF);
    out[12] = '\0';
}

static void get_brand(char out[49])
{
    uint32_t a, b, c, d, max_ext;
    cpuid_raw(0x80000000u, 0, &max_ext, &b, &c, &d);
    if (max_ext < 0x80000004u) { out[0] = '\0'; return; }

    for (uint32_t i = 0; i < 3; i++) {
        cpuid_raw(0x80000002u + i, 0, &a, &b, &c, &d);
        char *p = out + i * 16;
        p[0]  = (char)( a        & 0xFF);
        p[1]  = (char)((a >>  8) & 0xFF);
        p[2]  = (char)((a >> 16) & 0xFF);
        p[3]  = (char)((a >> 24) & 0xFF);
        p[4]  = (char)( b        & 0xFF);
        p[5]  = (char)((b >>  8) & 0xFF);
        p[6]  = (char)((b >> 16) & 0xFF);
        p[7]  = (char)((b >> 24) & 0xFF);
        p[8]  = (char)( c        & 0xFF);
        p[9]  = (char)((c >>  8) & 0xFF);
        p[10] = (char)((c >> 16) & 0xFF);
        p[11] = (char)((c >> 24) & 0xFF);
        p[12] = (char)( d        & 0xFF);
        p[13] = (char)((d >>  8) & 0xFF);
        p[14] = (char)((d >> 16) & 0xFF);
        p[15] = (char)((d >> 24) & 0xFF);
    }
    out[48] = '\0';

    int i = 0, j = 0;
    while (out[i] == ' ') i++;
    while (out[i]) out[j++] = out[i++];
    out[j] = '\0';

    while (j > 0 && (out[j-1] == ' ' || out[j-1] == '\t')) out[--j] = '\0';
}

/* ============================================================
 * Collected CPU data
 * ============================================================ */

typedef struct {
    char     vendor[13];
    char     brand[49];
    uint32_t family, model, stepping;
    uint32_t ext_family, ext_model;
    uint32_t logical;
    uint32_t phys;
    uint32_t tpc;
    uint32_t base_mhz, max_mhz, bus_mhz;
    uint32_t feat_edx, feat_ecx, feat_ebx7, feat_ecx7;
    uint32_t max_leaf, max_ext_leaf;
    int      has_topology_0B;
} cpu_t;

static void cpu_collect(cpu_t *ci)
{
    uint32_t a, b, c, d;

    get_vendor(ci->vendor);
    get_brand (ci->brand);

    cpuid_raw(0, 0, &ci->max_leaf, &b, &c, &d);
    cpuid_raw(0x80000000u, 0, &ci->max_ext_leaf, &b, &c, &d);

    cpuid_raw(1, 0, &a, &b, &c, &d);
    ci->stepping   =  a        & 0x0F;
    ci->model      = (a >> 4)  & 0x0F;
    ci->family     = (a >> 8)  & 0x0F;
    ci->ext_model  = (a >> 16) & 0x0F;
    ci->ext_family = (a >> 20) & 0xFF;

    ci->feat_edx = d;
    ci->feat_ecx = c;

    ci->logical = (b >> 16) & 0xFF;
    if (ci->logical == 0) ci->logical = 1;

    uint32_t smt = 0, cores = 0;
    ci->has_topology_0B = 0;
    if (ci->max_leaf >= 0x0B) {
        for (uint32_t i = 0; i < 4; i++) {
            cpuid_raw(0x0B, i, &a, &b, &c, &d);
            uint32_t type  = (c >> 8) & 0xFF;
            uint32_t level =  b       & 0xFFFF;
            if (type == 0 && level == 0) break;
            if (type == 1) smt   = level;
            if (type == 2) cores = level;
        }
        if (cores) ci->has_topology_0B = 1;
    }
    if (cores == 0 && ci->max_leaf >= 4) {
        cpuid_raw(4, 0, &a, &b, &c, &d);
        cores = ((a >> 26) & 0x3F) + 1;
    }
    ci->phys = cores ? cores : ci->logical;
    ci->tpc  = (smt && cores) ? smt : (ci->logical / ci->phys);
    if (ci->tpc == 0) ci->tpc = 1;

    if (ci->max_leaf >= 0x16) {
        cpuid_raw(0x16, 0, &a, &b, &c, &d);
        ci->base_mhz =  a        & 0xFFFF;
        ci->max_mhz  = (a >> 16) & 0xFFFF;
        ci->bus_mhz  =  b        & 0xFFFF;
    } else {
        ci->base_mhz = ci->max_mhz = ci->bus_mhz = 0;
    }

    if (ci->max_leaf >= 7) {
        cpuid_raw(7, 0, &a, &b, &c, &d);
        ci->feat_ebx7 = b;
        ci->feat_ecx7 = c;
    } else {
        ci->feat_ebx7 = ci->feat_ecx7 = 0;
    }
}

/* ============================================================
 * COMMAND: cpuid — identification
 * ============================================================ */

void cpuid(void)
{
    cpu_t ci;
    cpu_collect(&ci);

    uint32_t fam = ci.family;
    uint32_t mod = ci.model;
    if (fam == 0x0F) fam += ci.ext_family;
    if (fam == 0x06 || fam == 0x0F) mod += (ci.ext_model << 4);

    header("CPU IDENTIFICATION");

    if (ci.brand[0]) field("Brand", ci.brand, C_WHITE);
    field("Vendor", ci.vendor, C_WHITE);

    buf_reset();
    buf_str("Family / Model / Step");
    buf_pad(22);
    buf_str(": ");
    buf_hex2(fam);  buf_ch('/');
    buf_hex2(mod);  buf_ch('/');
    buf_hex2(ci.stepping);
    buf_flush(C_WHITE);

    buf_reset();
    buf_str("Raw family / ext family");
    buf_pad(22);
    buf_str(": ");
    buf_hex2(ci.family); buf_str("h / ");
    buf_hex2(ci.ext_family); buf_str("h");
    buf_flush(C_GRAY);

    buf_reset();
    buf_str("Raw model / ext model");
    buf_pad(22);
    buf_str(": ");
    buf_hex2(ci.model); buf_str("h / ");
    buf_hex2(ci.ext_model); buf_str("h");
    buf_flush(C_GRAY);

    {
        uint32_t a, b, c, d;
        cpuid_raw(1, 0, &a, &b, &c, &d);
        buf_reset();
        buf_str("Signature (EAX)");
        buf_pad(22);
        buf_str(": 0x");
        buf_hex2((a >> 24) & 0xFF);
        buf_hex2((a >> 16) & 0xFF);
        buf_hex2((a >>  8) & 0xFF);
        buf_hex2( a        & 0xFF);
        buf_flush(C_GRAY);
    }

    print("==========================================\n", C_CYAN);
}

/* ============================================================
 * COMMAND: fcpuid — full dump
 * ============================================================ */

static void print_caches(void)
{
    uint32_t a, b, c, d, max_leaf;
    cpuid_raw(0, 0, &max_leaf, &b, &c, &d);
    if (max_leaf < 4) return;

    print("Caches:\n", C_YELLOW);

    for (uint32_t i = 0; i < 16; i++) {
        cpuid_raw(4, i, &a, &b, &c, &d);
        uint32_t type = a & 0x1F;
        if (type == 0) break;

        uint32_t level     = (a >> 5) & 0x7;
        uint32_t line_size = (b & 0xFFF) + 1;
        uint32_t ways      = ((b >> 22) & 0x3FF) + 1;
        uint32_t sets      = c + 1;
        uint32_t size_kb   = (ways * line_size * sets) / 1024;

        const char *tname = (type == 1) ? "Data   " :
                            (type == 2) ? "Instr  " :
                            (type == 3) ? "Unified" : "?      ";

        buf_reset();
        buf_str("  L"); buf_u32(level); buf_ch(' ');
        buf_str(tname); buf_str("  ");
        buf_u32(size_kb); buf_str(" KB, line ");
        buf_u32(line_size); buf_str(" B, ");
        buf_u32(ways); buf_str("-way, ");
        buf_u32(sets); buf_str(" sets");
        buf_flush(C_GRAY);
    }
}

static void print_features(void)
{
    uint32_t a, b, c, d, max_leaf;
    cpuid_raw(0, 0, &max_leaf, &b, &c, &d);
    cpuid_raw(1, 0, &a, &b, &c, &d);
    uint32_t edx = d, ecx = c;
    uint32_t ebx7 = 0, ecx7 = 0;
    if (max_leaf >= 7) {
        cpuid_raw(7, 0, &a, &b, &c, &d);
        ebx7 = b; ecx7 = c;
    }

    print("Features:\n", C_YELLOW);

    #define F(name, cond) field(name, (cond) ? "yes" : "no", \
                                (cond) ? C_GREEN : C_RED)

    F("FPU",     edx & (1u <<  0));
    F("TSC",     edx & (1u <<  4));
    F("MSR",     edx & (1u <<  5));
    F("APIC",    edx & (1u <<  9));
    F("CMOV",    edx & (1u << 15));
    F("MMX",     edx & (1u << 23));
    F("SSE",     edx & (1u << 25));
    F("SSE2",    edx & (1u << 26));
    F("HTT",     edx & (1u << 28));
    F("SSE3",    ecx & (1u <<  0));
    F("SSSE3",   ecx & (1u <<  9));
    F("CX16",    ecx & (1u << 13));
    F("SSE4.1",  ecx & (1u << 19));
    F("SSE4.2",  ecx & (1u << 20));
    F("POPCNT",  ecx & (1u << 23));
    F("AES-NI",  ecx & (1u << 25));
    F("XSAVE",   ecx & (1u << 26));
    F("OSXSAVE", ecx & (1u << 27));
    F("AVX",     ecx & (1u << 28));
    F("F16C",    ecx & (1u << 29));
    F("RDRAND",  ecx & (1u << 30));
    F("Hypervisor", ecx & (1u << 31));

    if (max_leaf >= 7) {
        F("BMI1",    ebx7 & (1u <<  3));
        F("AVX2",    ebx7 & (1u <<  5));
        F("BMI2",    ebx7 & (1u <<  8));
        F("ERMS",    ebx7 & (1u <<  9));
        F("AVX512F", ebx7 & (1u << 16));
        F("RDSEED",  ebx7 & (1u << 18));
        F("ADX",     ebx7 & (1u << 19));
        F("SMAP",    ebx7 & (1u << 20));
        F("CLFLUSHOPT", ebx7 & (1u << 23));
        F("SHA",     ebx7 & (1u << 29));
        F("AVX512BW",ebx7 & (1u << 30));
    }
    #undef F
}

void fcpuid(void)
{
    cpu_t ci;
    cpu_collect(&ci);

    uint32_t fam = ci.family;
    uint32_t mod = ci.model;
    if (fam == 0x0F) fam += ci.ext_family;
    if (fam == 0x06 || fam == 0x0F) mod += (ci.ext_model << 4);

    header("FULL CPU INFO");

    print("Identification:\n", C_YELLOW);
    if (ci.brand[0]) field("Brand", ci.brand, C_WHITE);
    field("Vendor", ci.vendor, C_WHITE);

    buf_reset();
    buf_str("Family / Model / Step");
    buf_pad(22);
    buf_str(": ");
    buf_hex2(fam); buf_ch('/');
    buf_hex2(mod); buf_ch('/');
    buf_hex2(ci.stepping);
    buf_flush(C_WHITE);

    field_u("Max CPUID leaf",     ci.max_leaf,     " (basic)",    C_GRAY);
    field_u("Max CPUID ext leaf", ci.max_ext_leaf, " (extended)", C_GRAY);
    blank();

    print("Topology:\n", C_YELLOW);
    field_u("Logical processors", ci.logical, NULL, C_WHITE);
    field_u("Physical cores",     ci.phys,    NULL, C_WHITE);
    field_u("Threads per core",   ci.tpc,     NULL, C_WHITE);
    blank();

    print("Frequency:\n", C_YELLOW);
    if (ci.base_mhz) field_u("Base",  ci.base_mhz, " MHz", C_WHITE);
    else             field("Base", "unknown (CPUID.0x16 not supported)", C_GRAY);
    if (ci.max_mhz)  field_u("Turbo", ci.max_mhz,  " MHz", C_WHITE);
    if (ci.bus_mhz)  field_u("Bus",   ci.bus_mhz,  " MHz", C_WHITE);
    blank();

    print_caches();
    blank();

    print_features();

    print("==========================================\n", C_CYAN);
}

/* ============================================================
 * COMMAND: hcpuid — hardware / microarchitecture
 * ============================================================ */

void hcpuid(void)
{
    cpu_t ci;
    cpu_collect(&ci);

    header("CPU HARDWARE");

    print("Core / Thread topology:\n", C_YELLOW);
    field_u("Logical processors", ci.logical, NULL, C_WHITE);
    field_u("Physical cores",     ci.phys,    NULL, C_WHITE);
    field_u("Threads per core",   ci.tpc,     NULL, C_WHITE);
    if (ci.tpc > 1)
        field("SMT / Hyper-Threading", "enabled", C_GREEN);
    else
        field("SMT / Hyper-Threading", "disabled / N/A", C_GRAY);

    {
        uint32_t a, b, c, d;
        cpuid_raw(1, 0, &a, &b, &c, &d);
        uint32_t apic_id = (b >> 24) & 0xFF;
        field_u("APIC ID (this core)", apic_id, NULL, C_GRAY);
    }

    if (ci.has_topology_0B) {
        blank();
        print("Extended topology (CPUID.0x0B):\n", C_YELLOW);
        for (uint32_t i = 0; i < 4; i++) {
            uint32_t a, b, c, d;
            cpuid_raw(0x0B, i, &a, &b, &c, &d);
            uint32_t type  = (c >> 8) & 0xFF;
            uint32_t level =  b       & 0xFFFF;
            if (type == 0 && level == 0) break;
            const char *tname = (type == 1) ? "SMT"    :
                                (type == 2) ? "Core"   :
                                (type == 3) ? "Module" :
                                (type == 4) ? "Tile"   :
                                (type == 5) ? "Die"    : "?";
            buf_reset();
            buf_str("  Level ");
            buf_u32(i);
            buf_str("  type=");
            buf_str(tname);
            buf_str("  count=");
            buf_u32(level);
            buf_flush(C_GRAY);
        }
    }

    blank();
    print_caches();

    {
        uint32_t a, b, c, d, max_leaf;
        cpuid_raw(0, 0, &max_leaf, &b, &c, &d);
        if (max_leaf >= 4) {
            for (uint32_t i = 0; i < 16; i++) {
                cpuid_raw(4, i, &a, &b, &c, &d);
                uint32_t type = a & 0x1F;
                uint32_t lev  = (a >> 5) & 0x7;
                if (type == 0) break;
                if (lev == 1 && type == 1) {
                    uint32_t line = (b & 0xFFF) + 1;
                    blank();
                    field_u("Cache line size", line, " bytes", C_WHITE);
                    break;
                }
            }
        }
    }

    blank();
    print("Frequency:\n", C_YELLOW);
    if (ci.base_mhz) field_u("Base",  ci.base_mhz, " MHz", C_WHITE);
    else             field("Base",  "unknown", C_GRAY);
    if (ci.max_mhz)  field_u("Turbo", ci.max_mhz,  " MHz", C_WHITE);
    if (ci.bus_mhz)  field_u("Bus",   ci.bus_mhz,  " MHz", C_WHITE);

    print("==========================================\n", C_CYAN);
}