#include "input.h"
#include "../video/video.h"

#define KBD_DATA_PORT    0x60
#define KBD_STATUS_PORT  0x64

static inline unsigned char inb(unsigned short port)
{
    unsigned char r;
    __asm__ volatile ("inb %1, %0" : "=a"(r) : "Nd"(port) : "memory");
    return r;
}

static int shift_pressed = -1;

static const char normal_ascii[128] = {
    [0x02]='1', [0x03]='2', [0x04]='3', [0x05]='4', [0x06]='5',
    [0x07]='6', [0x08]='7', [0x09]='8', [0x0A]='9', [0x0B]='0',
    [0x0C]='-', [0x0D]='=', [0x0E]='\b', [0x0F]='\t',
    [0x10]='q', [0x11]='w', [0x12]='e', [0x13]='r', [0x14]='t',
    [0x15]='y', [0x16]='u', [0x17]='i', [0x18]='o', [0x19]='p',
    [0x1A]='[', [0x1B]=']', [0x1C]='\n',
    [0x1E]='a', [0x1F]='s', [0x20]='d', [0x21]='f', [0x22]='g',
    [0x23]='h', [0x24]='j', [0x25]='k', [0x26]='l',
    [0x27]=';', [0x28]='\'', [0x29]='`', [0x2B]='\\',
    [0x2C]='z', [0x2D]='x', [0x2E]='c', [0x2F]='v', [0x30]='b',
    [0x31]='n', [0x32]='m', [0x33]=',', [0x34]='.', [0x35]='/',
    [0x37]='*', [0x39]=' ',
};

static const char shift_ascii[128] = {
    [0x02]='!', [0x03]='@', [0x04]='#', [0x05]='$', [0x06]='%',
    [0x07]='^', [0x08]='&', [0x09]='*', [0x0A]='(', [0x0B]=')',
    [0x0C]='_', [0x0D]='+', [0x0E]='\b', [0x0F]='\t',
    [0x10]='Q', [0x11]='W', [0x12]='E', [0x13]='R', [0x14]='T',
    [0x15]='Y', [0x16]='U', [0x17]='I', [0x18]='O', [0x19]='P',
    [0x1A]='{', [0x1B]='}', [0x1C]='\n',
    [0x1E]='A', [0x1F]='S', [0x20]='D', [0x21]='F', [0x22]='G',
    [0x23]='H', [0x24]='J', [0x25]='K', [0x26]='L',
    [0x27]=':', [0x28]='"', [0x29]='~', [0x2B]='|',
    [0x2C]='Z', [0x2D]='X', [0x2E]='C', [0x2F]='V', [0x30]='B',
    [0x31]='N', [0x32]='M', [0x33]='<', [0x34]='>', [0x35]='?',
    [0x37]='*', [0x39]=' ',
};

/* =========================================================
 *  Локальное эхо — рисует символ + сбрасывает буфер на экран
 * ========================================================= */
static void echo_char(char c, unsigned char color)
{
    if (c == '\n') {
        int x, y;
        video_get_cursor(&x, &y);
        video_set_cursor(0, y + 1);
        video_flush();                       /* ← сброс */
        return;
    }
    if (c == '\r') {
        int x, y;
        video_get_cursor(&x, &y);
        video_set_cursor(0, y);
        video_flush();
        return;
    }
    if (c == '\b') {
        int x, y;
        video_get_cursor(&x, &y);
        if (x > 0) {
            x--;
            putchar_xy(x, y, ' ', color);
            video_set_cursor(x, y);
        }
        video_flush();                       /* ← сброс */
        return;
    }

    int x, y;
    video_get_cursor(&x, &y);

    putchar_xy(x, y, (unsigned int)(unsigned char)c, color);

    x++;
    if (x >= COLS) { x = 0; y++; }
    if (y >= ROWS) { y = ROWS - 1; x = 0; }
    video_set_cursor(x, y);

    video_flush();                           /* ← ГЛАВНОЕ: сброс после каждого символа */
}

/* =========================================================
 *  API
 * ========================================================= */

void input_init(void)
{
    shift_pressed = 0;
}

char getchar(void)
{
    if (shift_pressed < 0) shift_pressed = 0;

    for (;;) {
        while ((inb(KBD_STATUS_PORT) & 0x01) == 0) { }

        unsigned char sc = inb(KBD_DATA_PORT);

        if (sc & 0x80) {
            unsigned char make = sc & 0x7F;
            if (make == 0x2A || make == 0x36)
                shift_pressed = 0;
            continue;
        }

        if (sc == 0x2A || sc == 0x36) {
            shift_pressed = 1;
            continue;
        }

        char c = shift_pressed ? shift_ascii[sc] : normal_ascii[sc];
        if (c) return c;
    }
}

char getchar_echo(unsigned char color)
{
    char c = getchar();
    echo_char(c, color);
    return c;
}

void read_line(char *buf, int max, unsigned char color)
{
    int i = 0;
    for (;;) {
        char c = getchar();

        if (c == '\n') {
            echo_char('\n', color);
            break;
        }
        if (c == '\b') {
            if (i > 0) {
                i--;
                echo_char('\b', color);
            }
            continue;
        }
        if (c == '\t') continue;

        if (i < max - 1) {
            buf[i++] = c;
            echo_char(c, color);
        }
    }
    buf[i] = '\0';
}

int read_string(char *buf, int max, unsigned char color)
{
    int i = 0;
    char c;

    do { c = getchar(); } while (c == ' ' || c == '\t');

    while (c != ' ' && c != '\t' && c != '\n') {
        if (i < max - 1) {
            buf[i++] = c;
            echo_char(c, color);
        }
        c = getchar();
    }

    echo_char(c, color);

    buf[i] = '\0';
    return i;
}