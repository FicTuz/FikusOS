/* ============================================================
 * meminfo.c — Memory info (self-detecting: CMOS + probing)
 *
 * No bootloader, no kernel hooks, no libgcc.
 *   - CMOS tells us conventional + extended up to 64 MB.
 *   - If CMOS saturates (ext == 0xFFFF), we probe physical RAM
 *     by walking down from 1 GB until we find RAM, then walking
 *     up in 4 KB steps to find the exact top.
 * ============================================================ */

#include <stdint.h>
#include <stddef.h>
#include "../video/video.h"
#include "meminfo.h"

#define C_WHITE   0x0F
#define C_GREEN   0x0A
#define C_YELLOW  0x0E
#define C_CYAN    0x0B
#define C_GRAY    0x07
#define C_RED     0x0C

/* ============================================================
 * I/O ports (CMOS)
 * ============================================================ */

static inline uint8_t inb(uint16_t port)
{
    uint8_t r;
    __asm__ __volatile__("inb %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}

static inline void outb(uint16_t port, uint8_t v)
{
    __asm__ __volatile__("outb %0, %1" : : "a"(v), "Nd"(port));
}

static uint8_t cmos_read(uint8_t reg)
{
    outb(0x70, reg);
    return inb(0x71);
}

/* ============================================================
 * 64-bit unsigned division (no libgcc, no __udivdi3)
 * ============================================================ */

static uint64_t udiv64(uint64_t n, uint64_t d, uint64_t *rem)
{
    if (d == 0) { if (rem) *rem = 0; return 0; }
    uint64_t q = 0, r = 0;
    for (int i = 63; i >= 0; --i) {
        r = (r << 1) | ((n >> i) & 1ULL);
        if (r >= d) { r -= d; q |= (1ULL << i); }
    }
    if (rem) *rem = r;
    return q;
}

/* ============================================================
 * String buffer
 * ============================================================ */

static char line[160];
static int  line_len;

static void buf_reset(void) { line_len = 0; line[0] = '\0'; }
static void buf_ch(char c) { if (line_len < (int)sizeof(line)-2) { line[line_len++] = c; line[line_len] = '\0'; } }
static void buf_str(const char *s) { while (*s) buf_ch(*s++); }

static void buf_u64(uint64_t v)
{
    char t[24]; int i = 0;
    if (v == 0) { buf_ch('0'); return; }
    while (v) { t[i++] = '0' + (char)(v % 10ULL); v /= 10ULL; }
    while (i--) buf_ch(t[i]);
}

static void buf_hex8(uint8_t v)
{
    const char h[] = "0123456789ABCDEF";
    buf_ch(h[(v >> 4) & 0xF]);
    buf_ch(h[ v       & 0xF]);
}

static void buf_hex64(uint64_t v)
{
    const char h[] = "0123456789ABCDEF";
    for (int i = 60; i >= 0; i -= 4) buf_ch(h[(v >> i) & 0xF]);
}

static void buf_pad(int width) { while (line_len < width) buf_ch(' '); }
static void buf_flush(uint8_t color) { buf_ch('\n'); print(line, color); }

static void buf_bytes(uint64_t b)
{
    if (b < 1024) { buf_u64(b); buf_str(" B"); return; }
    uint64_t whole, frac; const char *unit;
    if (b >= (1ULL << 30)) {
        whole = b >> 30;
        frac  = udiv64((b & ((1ULL << 30) - 1)) * 100ULL, 1ULL << 30, NULL);
        unit  = " GB";
    } else if (b >= (1ULL << 20)) {
        whole = b >> 20;
        frac  = udiv64((b & ((1ULL << 20) - 1)) * 100ULL, 1ULL << 20, NULL);
        unit  = " MB";
    } else {
        whole = b >> 10;
        frac  = udiv64((b & ((1ULL << 10) - 1)) * 100ULL, 1ULL << 10, NULL);
        unit  = " KB";
    }
    buf_u64(whole); buf_ch('.');
    if (frac < 10) buf_ch('0');
    buf_u64(frac); buf_str(unit);
}

/* ============================================================
 * helpers
 * ============================================================ */

static void blank(void) { print("\n", C_WHITE); }

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

static void field(const char *label, const char *value, uint8_t color)
{
    buf_reset(); buf_str(label); buf_pad(22); buf_str(": "); buf_str(value); buf_flush(color);
}

static void field_u64(const char *label, uint64_t v, const char *suffix, uint8_t color)
{
    buf_reset(); buf_str(label); buf_pad(22); buf_str(": ");
    buf_u64(v); if (suffix) buf_str(suffix); buf_flush(color);
}

static void field_bytes(const char *label, uint64_t b, uint8_t color)
{
    buf_reset(); buf_str(label); buf_pad(22); buf_str(": "); buf_bytes(b); buf_flush(color);
}

static void field_bytes_full(const char *label, uint64_t b, uint8_t color)
{
    buf_reset(); buf_str(label); buf_pad(22); buf_str(": ");
    buf_bytes(b); buf_str("  ("); buf_u64(b); buf_str(" bytes)");
    buf_flush(color);
}

/* ============================================================
 * CMOS detection
 * ============================================================ */

typedef struct {
    uint32_t base_kb;
    uint32_t ext_cmos_kb;
    uint8_t  raw15, raw16, raw17, raw18;
    int      saturated;   /* ext_cmos_kb == 0xFFFF */
} mem_t;

static void mem_cmos(mem_t *m)
{
    m->raw15 = cmos_read(0x15);
    m->raw16 = cmos_read(0x16);
    m->raw17 = cmos_read(0x17);
    m->raw18 = cmos_read(0x18);

    m->base_kb     = ((uint32_t)m->raw16 << 8) | m->raw15;
    m->ext_cmos_kb = ((uint32_t)m->raw18 << 8) | m->raw17;
    m->saturated   = (m->ext_cmos_kb == 0xFFFF);
}

/* ============================================================
 * Memory probing
 *
 * Safe technique: save 4 bytes, write 0x5A5AA5A5, verify,
 * write complement 0xA5A55A5A, verify, restore.
 * Both patterns must match for the address to be real RAM —
 * this rejects MMIO that might hold the last written value.
 * ============================================================ */

static int is_ram(uint64_t addr)
{
    volatile uint32_t *p = (volatile uint32_t *)(uintptr_t)addr;
    uint32_t saved = *p;

    *p = 0x5A5AA5A5u;
    if (*p != 0x5A5AA5A5u) { *p = saved; return 0; }
    *p = 0xA5A55A5Au;
    if (*p != 0xA5A55A5Au) { *p = saved; return 0; }
    *p = saved;
    return 1;
}

/* Walk down from 1 GB in 1 MB steps to find first RAM, then up
 * in 4 KB steps to find the exact top. Capped at 1 GB. */
static uint64_t probe_top(void)
{
    const uint64_t SEARCH_MAX = 0x40000000ULL;  /* 1 GB cap */
    const uint64_t MIN_ADDR   = 0x00100000ULL;  /* 1 MB     */
    const uint64_t BIG        = 0x00100000ULL;  /* 1 MB     */
    const uint64_t SMALL      = 0x00001000ULL;  /* 4 KB     */

    uint64_t a = SEARCH_MAX - BIG;
    while (a >= MIN_ADDR) {
        if (is_ram(a)) break;
        a -= BIG;
    }
    if (a < MIN_ADDR) return 0;

    uint64_t top = a;
    while (top + SMALL <= SEARCH_MAX && is_ram(top + SMALL))
        top += SMALL;

    return top + SMALL;
}

/* ============================================================
 * COMMAND: meminfo — brief
 * ============================================================ */

void meminfo(void)
{
    mem_t m;
    mem_cmos(&m);

    header("MEMORY INFO");

    field_bytes_full("Conventional", (uint64_t)m.base_kb * 1024ULL, C_WHITE);

    if (!m.saturated) {
        /* CMOS is trustworthy */
        field_bytes_full("Extended", (uint64_t)m.ext_cmos_kb * 1024ULL, C_WHITE);
        uint64_t total = ((uint64_t)m.base_kb + (uint64_t)m.ext_cmos_kb) * 1024ULL;
        field_bytes_full("Total", total, C_GREEN);
    } else {
        /* CMOS saturated at 64 MB — probe real top */
        field("Extended", "CMOS saturates at 64 MB", C_YELLOW);
        uint64_t top = probe_top();
        if (top) {
            field_bytes_full("Total (probed)", top, C_GREEN);
        } else {
            field("Total", "probe failed", C_RED);
        }
    }

    field("Free", "N/A (no allocator yet)", C_GRAY);

    print("==========================================\n", C_CYAN);
}

/* ============================================================
 * COMMAND: fmeminfo — full
 * ============================================================ */

void fmeminfo(void)
{
    mem_t m;
    mem_cmos(&m);

    header("FULL MEMORY INFO");

    print("CMOS registers:\n", C_YELLOW);
    {
        buf_reset();
        buf_str("  0x15 / 0x16  (base KB)");
        buf_pad(30);
        buf_str(": 0x"); buf_hex8(m.raw16); buf_hex8(m.raw15);
        buf_str("  = "); buf_u64(m.base_kb); buf_str(" KB");
        buf_flush(C_GRAY);
    }
    {
        buf_reset();
        buf_str("  0x17 / 0x18  (ext KB) ");
        buf_pad(30);
        buf_str(": 0x"); buf_hex8(m.raw18); buf_hex8(m.raw17);
        buf_str("  = "); buf_u64(m.ext_cmos_kb); buf_str(" KB");
        if (m.saturated) buf_str("   [SATURATED at 64 MB]");
        buf_flush(m.saturated ? C_RED : C_GRAY);
    }
    blank();

    print("Detected memory:\n", C_YELLOW);
    field_bytes("Conventional", (uint64_t)m.base_kb * 1024ULL, C_WHITE);

    if (!m.saturated) {
        field_bytes("Extended (CMOS)", (uint64_t)m.ext_cmos_kb * 1024ULL, C_WHITE);
        uint64_t total = ((uint64_t)m.base_kb + (uint64_t)m.ext_cmos_kb) * 1024ULL;
        field_bytes("Total", total, C_GREEN);
    } else {
        field("Extended (CMOS)", "saturated at 64 MB", C_YELLOW);
        print("Probing physical memory (this takes a moment)...\n", C_GRAY);
        uint64_t top = probe_top();
        if (top) {
            field_bytes_full("Total (probed)", top, C_GREEN);
        } else {
            field("Total (probed)", "failed", C_RED);
        }
    }

    print("==========================================\n", C_CYAN);
}

/* ============================================================
 * COMMAND: hmeminfo — hardware
 * ============================================================ */

void hmeminfo(void)
{
    mem_t m;
    mem_cmos(&m);

    header("MEMORY HARDWARE");

    field_u64("Page size", 4096, " bytes", C_WHITE);
    field_bytes("Conventional end", 0xA0000ULL, C_GRAY);
    field_bytes("Extended start",   0x100000ULL, C_GRAY);

    if (!m.saturated) {
        uint64_t total = ((uint64_t)m.base_kb + (uint64_t)m.ext_cmos_kb) * 1024ULL;
        field_bytes("Total RAM (CMOS)", total, C_GREEN);
    } else {
        uint64_t top = probe_top();
        if (top) field_bytes("Total RAM (probed)", top, C_GREEN);
        else     field("Total RAM", "unknown", C_RED);
    }

    blank();
    print("Typical physical layout:\n", C_YELLOW);
    print("  0x00000000 - 0x0009FBFF   conventional (usable)\n", C_GREEN);
    print("  0x0009FC00 - 0x0009FFFF   EBDA (reserved)\n", C_GRAY);
    print("  0x000A0000 - 0x000BFFFF   VGA (MMIO)\n", C_GRAY);
    print("  0x000C0000 - 0x000FFFFF   BIOS ROM (reserved)\n", C_GRAY);
    print("  0x00100000 - ...          extended (usable)\n", C_GREEN);

    print("==========================================\n", C_CYAN);
}