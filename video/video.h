#ifndef VIDEO_H
#define VIDEO_H

#include <stdint.h>

#define VGA_WIDTH   640
#define VGA_HEIGHT  480

#define CHAR_W      8
#define CHAR_H      8

#define COLS        (VGA_WIDTH  / CHAR_W)
#define ROWS        (VGA_HEIGHT / CHAR_H)

#define COLOR_BLACK        0
#define COLOR_BLUE         1
#define COLOR_GREEN        2
#define COLOR_CYAN         3
#define COLOR_RED          4
#define COLOR_MAGENTA      5
#define COLOR_BROWN        6
#define COLOR_LIGHTGRAY    7
#define COLOR_DARKGRAY     8
#define COLOR_LIGHTBLUE    9
#define COLOR_LIGHTGREEN   10
#define COLOR_LIGHTCYAN    11
#define COLOR_LIGHTRED     12
#define COLOR_LIGHTMAGENTA 13
#define COLOR_YELLOW       14
#define COLOR_WHITE        15

#define VGA_COLOR(fg, bg)  (((bg) << 4) | (fg))
#define VGA_DEFAULT        0x07

/* =========================================================
 *  Базовое
 * ========================================================= */
void video_init(void);
void video_clear(unsigned char color);
void video_flush(void);

void putpixel(int x, int y, unsigned char color);
unsigned char getpixel(int x, int y);

/* =========================================================
 *  Вывод текста
 * ========================================================= */
void putchar_xy(int x, int y, unsigned int codepoint, unsigned char color);
void putchar(char c, unsigned char color);

void print(const char *s, unsigned char color);
void print_xy(int x, int y, const char *s, unsigned char color);

/* Число в строку (внутри — статический буфер) */
void print_int(int v, unsigned char color);
void print_hex(unsigned int v, unsigned char color);
void print_bin(unsigned int v, unsigned char color);

/* =========================================================
 *  Большой текст: scale = 1..8 (1 = обычный, 2..8 = увеличение)
 * ========================================================= */
void printB(const char *s, unsigned char color, int scale);
void printB_xy(int x, int y, const char *s, unsigned char color, int scale);

/* =========================================================
 *  Курсор
 * ========================================================= */
void video_set_cursor(int x, int y);
void video_get_cursor(int *x, int *y);
void video_scroll_up(unsigned char bg_color);

/* =========================================================
 *  Графика
 * ========================================================= */
void video_fill(int x, int y, int w, int h, unsigned char color);
void video_rect(int x, int y, int w, int h, unsigned char color);
void video_box3d(int x, int y, int w, int h,
                 unsigned char light, unsigned char face, unsigned char shadow,
                 int raised);
void video_hline(int x, int y, int w, unsigned char color);
void video_vline(int x, int y, int h, unsigned char color);

/* Круг (алгоритм Брезенхема) */
void video_circle(int cx, int cy, int r, unsigned char color);

/* Линия (Брезенхем) */
void video_line(int x0, int y0, int x1, int y1, unsigned char color);

const unsigned char *video_glyph(unsigned int codepoint);

#endif
