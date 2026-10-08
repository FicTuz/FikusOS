#include "mouse.h"
#include "../video/video.h"

#define MOUSE_DATA   0x60
#define MOUSE_STATUS 0x64
#define MOUSE_CMD    0x64

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port) : "memory");
}
static inline uint8_t inb(uint16_t port) {
    uint8_t r; __asm__ volatile ("inb %1, %0" : "=a"(r) : "Nd"(port) : "memory"); return r;
}

static void mouse_wait_write(void) {
    for (int i = 0; i < 100000; i++)
        if ((inb(MOUSE_STATUS) & 0x02) == 0) return;
}
static void mouse_wait_read(void) {
    for (int i = 0; i < 100000; i++)
        if ((inb(MOUSE_STATUS) & 0x01) != 0) return;
}
static void mouse_write(uint8_t val) {
    mouse_wait_write();
    outb(MOUSE_CMD, 0x64);
    mouse_wait_write();
    outb(MOUSE_DATA, val);
}
static uint8_t mouse_read(void) {
    mouse_wait_read();
    return inb(MOUSE_DATA);
}

static mouse_state_t state = { 320, 240, 0, 0, 0, 0, 0 };

static uint8_t packet[3];
static int mouse_cycle = 0;

void mouse_irq_handler(void)
{
    uint8_t status = inb(MOUSE_STATUS);
    if (!(status & 0x20)) return;
    uint8_t b = inb(MOUSE_DATA);

    if (mouse_cycle == 0) {
        if (!(b & 0x08)) return;
        packet[0] = b;
        mouse_cycle = 1;
    } else if (mouse_cycle == 1) {
        packet[1] = b;
        mouse_cycle = 2;
    } else {
        packet[2] = b;
        mouse_cycle = 0;

        uint8_t flags = packet[0];
        int dx = packet[1];
        int dy = packet[2];

        if (flags & 0x10) dx |= 0xFFFFFF00;
        if (flags & 0x20) dy |= 0xFFFFFF00;

        state.dx = dx;
        state.dy = -dy;
        state.left   = (flags & 0x01) ? 1 : 0;
        state.right  = (flags & 0x02) ? 1 : 0;
        state.middle = (flags & 0x04) ? 1 : 0;
    }
}

void mouse_poll(void)
{
    state.x += state.dx;
    state.y += state.dy;
    state.dx = 0;
    state.dy = 0;

    if (state.x < 0) state.x = 0;
    if (state.y < 0) state.y = 0;
    if (state.x >= VGA_WIDTH)  state.x = VGA_WIDTH  - 1;
    if (state.y >= VGA_HEIGHT) state.y = VGA_HEIGHT - 1;
}

mouse_state_t *mouse_get(void) { return &state; }

static const uint8_t cursor_data[16][12] = {
    {1,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0,0},
    {1,2,2,1,0,0,0,0,0,0,0,0},
    {1,2,2,2,1,0,0,0,0,0,0,0},
    {1,2,2,2,2,1,0,0,0,0,0,0},
    {1,2,2,2,2,2,1,0,0,0,0,0},
    {1,2,2,2,2,2,2,1,0,0,0,0},
    {1,2,2,2,2,2,2,2,1,0,0,0},
    {1,2,2,2,2,2,2,2,2,1,0,0},
    {1,2,2,2,2,1,1,1,1,1,1,0},
    {1,2,2,1,2,2,1,0,0,0,0,0},
    {1,2,1,0,1,2,2,1,0,0,0,0},
    {1,1,0,0,1,2,2,1,0,0,0,0},
    {1,0,0,0,0,1,2,1,0,0,0,0},
    {0,0,0,0,0,1,1,0,0,0,0,0},
};

/* Сохранённый фон под курсором */
static uint8_t saved_bg[16][12];
static int cursor_saved = 0;
static int saved_x = 0;
static int saved_y = 0;

void mouse_erase_cursor(void)
{
    if (!cursor_saved) return;

    for (int j = 0; j < 16; j++) {
        for (int i = 0; i < 12; i++) {
            int x = saved_x + i;
            int y = saved_y + j;
            video_putpixel_buffer(x, y, saved_bg[j][i]);
        }
    }
    cursor_saved = 0;
}

void mouse_draw_cursor(void)
{
    int cx = state.x;
    int cy = state.y;

    /* Сохранить фон под курсором */
    for (int j = 0; j < 16; j++) {
        for (int i = 0; i < 12; i++) {
            saved_bg[j][i] = video_getpixel_buffer(cx + i, cy + j);
        }
    }
    saved_x = cx;
    saved_y = cy;
    cursor_saved = 1;

    /* Нарисовать курсор */
    for (int j = 0; j < 16; j++) {
        for (int i = 0; i < 12; i++) {
            uint8_t v = cursor_data[j][i];
            if (v == 0) continue;
            unsigned char color = (v == 1) ? 0x00 : 0x0F;
            putpixel(cx + i, cy + j, color);
        }
    }
}

void mouse_init(void)
{
    mouse_wait_write();
    outb(MOUSE_CMD, 0xA8);

    mouse_write(0xF6);
    mouse_read();

    mouse_write(0xF4);
    mouse_read();

    state.x = VGA_WIDTH  / 2;
    state.y = VGA_HEIGHT / 2;
}