#include "time.h"
#include "../video/video.h"

/* =========================================================
 *  Port I/O
 * ========================================================= */
static inline void outb(unsigned short port, unsigned char val)
{
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port) : "memory");
}

static inline unsigned char inb(unsigned short port)
{
    unsigned char r;
    __asm__ volatile ("inb %1, %0" : "=a"(r) : "Nd"(port) : "memory");
    return r;
}

/* =========================================================
 *  PIT delay — чтение счётчика channel 0
 * ========================================================= */
static unsigned int pit_read_ch0(void)
{
    outb(0x43, 0x00);           /* latch counter 0 */
    unsigned char lo = inb(0x40);
    unsigned char hi = inb(0x40);
    return ((unsigned int)hi << 8) | lo;
}

void delay_us(unsigned int us)
{
    while (us > 0) {
        unsigned int chunk  = (us > 20000) ? 20000 : us;
        unsigned int target = chunk + chunk / 5;    /* ×1.2 = 1.193 МГц */
        unsigned int acc    = 0;
        unsigned int prev   = pit_read_ch0();

        while (acc < target) {
            unsigned int now = pit_read_ch0();
            if (now <= prev)
                acc += prev - now;
            else
                acc += prev + (65536u - now);
            prev = now;
        }
        us -= chunk;
    }
}

void delay_ms(unsigned int ms)
{
    while (ms > 0) {
        unsigned int chunk = (ms > 20) ? 20 : ms;
        delay_us(chunk * 1000);
        ms -= chunk;
    }
}

/* =========================================================
 *  RTC / CMOS
 * ========================================================= */
static unsigned char rtc_reg_read(unsigned char reg)
{
    outb(0x70, reg);
    return inb(0x71);
}

static int rtc_updating(void)
{
    outb(0x70, 0x0A);
    return inb(0x71) & 0x80;
}

static unsigned char bcd_to_bin(unsigned char bcd)
{
    return (unsigned char)((bcd & 0x0F) + ((bcd >> 4) * 10));
}

void rtc_read(struct rtc_time *t)
{
    while (rtc_updating()) { }

    unsigned char sec  = rtc_reg_read(0x00);
    unsigned char min  = rtc_reg_read(0x02);
    unsigned char hour = rtc_reg_read(0x04);
    unsigned char day  = rtc_reg_read(0x07);
    unsigned char mon  = rtc_reg_read(0x08);
    unsigned char year = rtc_reg_read(0x09);
    unsigned char sb   = rtc_reg_read(0x0B);

    int binary = (sb & 0x04) != 0;
    int hour24 = (sb & 0x02) != 0;
    int pm     = 0;

    if (!hour24) {
        pm    = (hour & 0x80) != 0;
        hour &= 0x7F;
    }

    if (!binary) {
        sec  = bcd_to_bin(sec);
        min  = bcd_to_bin(min);
        hour = bcd_to_bin(hour);
        day  = bcd_to_bin(day);
        mon  = bcd_to_bin(mon);
        year = bcd_to_bin(year);
    }

    if (!hour24 && pm && hour < 12)    hour += 12;
    if (!hour24 && !pm && hour == 12)  hour  = 0;

    t->second = sec;
    t->minute = min;
    t->hour   = hour;
    t->day    = day;
    t->month  = mon;
    t->year   = 2000u + year;
}

/* =========================================================
 *  Печать — через video API (putchar_xy + курсор)
 * ========================================================= */

/* Вывести один ASCII-символ и сдвинуть курсор */
static void out_char(char c, unsigned char color)
{
    int x, y;
    video_get_cursor(&x, &y);
    putchar_xy(x, y, (unsigned int)(unsigned char)c, color);

    x++;
    if (x >= COLS) { x = 0; y++; }
    if (y >= ROWS) { y = ROWS - 1; x = 0; }
    video_set_cursor(x, y);
}

static void out_2digits(unsigned int v, unsigned char color)
{
    out_char((char)('0' + (v / 10) % 10), color);
    out_char((char)('0' +  v % 10),       color);
}

static void out_4digits(unsigned int v, unsigned char color)
{
    out_char((char)('0' + (v / 1000) % 10), color);
    out_char((char)('0' + (v / 100)  % 10), color);
    out_char((char)('0' + (v / 10)   % 10), color);
    out_char((char)('0' +  v         % 10), color);
}

void time_print(const struct rtc_time *t, unsigned char color)
{
    out_4digits(t->year, color);
    out_char('-', color);
    out_2digits(t->month,  color);
    out_char('-', color);
    out_2digits(t->day,    color);
    out_char(' ', color);
    out_2digits(t->hour,   color);
    out_char(':', color);
    out_2digits(t->minute, color);
    out_char(':', color);
    out_2digits(t->second, color);
    out_char('\n', color);
}