#include "intr.h"
#include "../video/video.h"
#include "../input/input.h"
#include "../time/time.h"
#include "../fs/ramfs.h"
#include "../system/call.h"

/* =========================================================
 *  IDT
 * ========================================================= */
struct idt_entry {
    uint16_t base_lo;
    uint16_t sel;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_hi;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

/* 129 записей: 32 ISR + 16 IRQ + 0x80 syscall */
#define IDT_SIZE 129

static struct idt_entry idt[IDT_SIZE];
static struct idt_ptr   idtp;

static int panic_lock = 0xDEAD;

extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);

extern void irq0(void);  extern void irq1(void);  extern void irq2(void);
extern void irq3(void);  extern void irq4(void);  extern void irq5(void);
extern void irq6(void);  extern void irq7(void);  extern void irq8(void);
extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void);
extern void irq15(void);

extern void isr128(void);

extern void reboot(void);

static const char *exc_names[32] = {
    "Divide by zero", "Debug", "NMI", "Breakpoint",
    "Overflow", "BOUND range exceeded", "Invalid opcode",
    "Device not available", "Double fault",
    "Coprocessor segment overrun", "Invalid TSS",
    "Segment not present", "Stack-segment fault",
    "General protection fault", "Page fault", "Reserved",
    "x87 FPU error", "Alignment check", "Machine check",
    "SIMD FPU exception", "Virtualization exception",
    "Control protection",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved",
    "VMM communication", "Security exception", "Reserved",
};

/* =========================================================
 *  Port I/O
 * ========================================================= */
static inline void outb(unsigned short port, unsigned char val)
{
    __asm__ volatile ("outb %0, %1"
                      : : "a"(val), "Nd"(port)
                      : "memory");
}

static inline unsigned char inb(unsigned short port)
{
    unsigned char r;
    __asm__ volatile ("inb %1, %0"
                      : "=a"(r) : "Nd"(port)
                      : "memory");
    return r;
}

static inline void io_wait(void) { outb(0x80, 0); }

/* =========================================================
 *  PIC
 * ========================================================= */
#define PIC1_CMD   0x20
#define PIC1_DATA  0x21
#define PIC2_CMD   0xA0
#define PIC2_DATA  0xA1
#define PIC_EOI    0x20

void pic_mask_all(void)
{
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
    io_wait();
}

void pic_remap(void)
{
    unsigned char mask1 = inb(PIC1_DATA);
    unsigned char mask2 = inb(PIC2_DATA);

    outb(PIC1_CMD,  0x11); io_wait();
    outb(PIC2_CMD,  0x11); io_wait();
    outb(PIC1_DATA, 0x20); io_wait();
    outb(PIC2_DATA, 0x28); io_wait();
    outb(PIC1_DATA, 0x04); io_wait();
    outb(PIC2_DATA, 0x02); io_wait();
    outb(PIC1_DATA, 0x01); io_wait();
    outb(PIC2_DATA, 0x01); io_wait();

    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

void pic_unmask(uint8_t irq)
{
    uint16_t port;
    uint8_t  mask;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
        uint8_t m = inb(PIC1_DATA) & ~(1u << 2);
        outb(PIC1_DATA, m);
    }
    mask = inb(port) & ~(1u << irq);
    outb(port, mask);
}

void pic_eoi(uint8_t irq)
{
    if (irq >= 8) outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

/* =========================================================
 *  IRQ
 * ========================================================= */
static irq_fn_t irq_handlers[16] = { 0 };

void irq_register(int irq, irq_fn_t fn)
{
    if (irq >= 0 && irq < 16) irq_handlers[irq] = fn;
}

void irq_enable(void)  { __asm__ volatile ("sti"); }
void irq_disable(void) { __asm__ volatile ("cli"); }

/* =========================================================
 *  IDT
 * ========================================================= */
static void set_gate(int n, uint32_t handler)
{
    if (n < 0 || n >= IDT_SIZE) return;
    idt[n].base_lo = (uint16_t)(handler & 0xFFFF);
    idt[n].base_hi = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[n].sel     = 0x08;
    idt[n].always0 = 0;
    idt[n].flags   = 0x8E;
}

void intr_init(void)
{
    __asm__ volatile ("cli");

    pic_remap();
    pic_mask_all();

    idtp.limit = (uint16_t)(sizeof(idt) - 1);
    idtp.base  = (uint32_t)&idt;

    uint32_t isr_stubs[32] = {
        (uint32_t)isr0,  (uint32_t)isr1,  (uint32_t)isr2,  (uint32_t)isr3,
        (uint32_t)isr4,  (uint32_t)isr5,  (uint32_t)isr6,  (uint32_t)isr7,
        (uint32_t)isr8,  (uint32_t)isr9,  (uint32_t)isr10, (uint32_t)isr11,
        (uint32_t)isr12, (uint32_t)isr13, (uint32_t)isr14, (uint32_t)isr15,
        (uint32_t)isr16, (uint32_t)isr17, (uint32_t)isr18, (uint32_t)isr19,
        (uint32_t)isr20, (uint32_t)isr21, (uint32_t)isr22, (uint32_t)isr23,
        (uint32_t)isr24, (uint32_t)isr25, (uint32_t)isr26, (uint32_t)isr27,
        (uint32_t)isr28, (uint32_t)isr29, (uint32_t)isr30, (uint32_t)isr31,
    };
    for (int i = 0; i < 32; i++) set_gate(i, isr_stubs[i]);

    uint32_t irq_stubs[16] = {
        (uint32_t)irq0,  (uint32_t)irq1,  (uint32_t)irq2,  (uint32_t)irq3,
        (uint32_t)irq4,  (uint32_t)irq5,  (uint32_t)irq6,  (uint32_t)irq7,
        (uint32_t)irq8,  (uint32_t)irq9,  (uint32_t)irq10, (uint32_t)irq11,
        (uint32_t)irq12, (uint32_t)irq13, (uint32_t)irq14, (uint32_t)irq15,
    };
    for (int i = 0; i < 16; i++) set_gate(32 + i, irq_stubs[i]);

    set_gate(0x80, (uint32_t)isr128);

    __asm__ volatile ("lidtl %0" : : "m"(idtp));
    __asm__ volatile ("cli");
}

/* =========================================================
 *  Syscall handler
 * ========================================================= */
void syscall_handler(struct registers *r)
{
    switch (r->eax) {

    /* ----- Linux ABI ----- */
    case SYS_WRITE: {
        /* write(fd, buf, count) */
        if (r->ebx == 1) {
            const char *s = (const char *)r->ecx;
            uint32_t n = r->edx;
            for (uint32_t i = 0; i < n; i++)
                putchar(s[i], 0x0F);
            r->eax = n;
        } else {
            r->eax = (uint32_t)-9;
        }
        break;
    }

    case SYS_READ: {
        if (r->ebx == 0) {
            read_line((char *)r->ecx, (int)r->edx, 0x0F);
            r->eax = 0;
        } else {
            r->eax = (uint32_t)-9;
        }
        break;
    }

    case SYS_EXIT:
        r->eax = 0;
        for (;;) __asm__ volatile ("hlt");

    case SYS_BRK:
        r->eax = r->ebx;
        break;

    case SYS_OPEN:
        r->eax = (uint32_t)-2;
        break;

    case SYS_CLOSE:
        r->eax = 0;
        break;

    case SYS_MMAP:
        r->eax = (uint32_t)-12;
        break;

    case SYS_MUNMAP:
        r->eax = 0;
        break;

    /* ----- FikusOS extensions ----- */
    case SYS_PRINT:
        print((const char *)r->ebx, (unsigned char)r->ecx);
        r->eax = 0;
        break;

    case SYS_PUTCHAR:
        putchar((char)r->ebx, (unsigned char)r->ecx);
        r->eax = 0;
        break;

    case SYS_CLEAR:
        video_clear((unsigned char)r->ebx);
        r->eax = 0;
        break;

    case SYS_GETCHAR:
        r->eax = (uint32_t)(unsigned char)getchar();
        break;

    case SYS_REBOOT:
        reboot();
        break;

    case SYS_SLEEP:
        delay_ms(r->ebx);
        r->eax = 0;
        break;

    case SYS_FS_CREATE:
        r->eax = (uint32_t)fs_create((const char *)r->ebx);
        break;

    case SYS_FS_LIST:
        fs_list();
        r->eax = 0;
        break;

    case SYS_FS_READ:
        r->eax = (uint32_t)fs_read((const char *)r->ebx,
                                   (void *)r->ecx, r->edx);
        break;

    case SYS_FS_WRITE:
        r->eax = (uint32_t)fs_write((const char *)r->ebx,
                                    (const void *)r->ecx, r->edx);
        break;

    default:
        r->eax = (uint32_t)-38;
        break;
    }
}

void syscall_init(void)
{
    /* Уже вызван в intr_init через set_gate(0x80, ...) */
}

/* =========================================================
 *  Panic
 * ========================================================= */
void panic(const char *msg, struct registers *r)
{
    __asm__ volatile ("cli");

    if (panic_lock == 0xCAFE) {
        for (;;) __asm__ volatile ("hlt");
    }
    panic_lock = 0xCAFE;

    volatile unsigned short *t = (volatile unsigned short *)0xB8000;
    for (int i = 0; i < 80 * 25; i++) t[i] = 0x1F20;

    const char *hdr = "*** KERNEL PANIC ***";
    for (int i = 0; hdr[i]; i++) t[i] = (unsigned short)hdr[i] | (0x4F << 8);

    for (int i = 0; msg && msg[i] && i < 80; i++)
        t[80 + i] = (unsigned short)msg[i] | (0x4F << 8);

    video_clear(0x4F);
    print("\n*** KERNEL PANIC ***\n", 0x4F);
    print("Reason: ", 0x4F);
    print(msg ? msg : "(null)", 0x4F);
    print("\n", 0x4F);

    if (r) {
        print("Vector:  ", 0x4F);
        print_int((int)r->int_no, 0x4F);
        print(" (", 0x4F);
        if (r->int_no < 32) print(exc_names[r->int_no], 0x4F);
        else                print("irq/hw", 0x4F);
        print(")\n", 0x4F);

        print("ErrCode: ", 0x4F); print_hex(r->err_code, 0x4F); print("\n", 0x4F);
        print("EIP:     ", 0x4F); print_hex(r->eip,      0x4F); print("\n", 0x4F);
        print("CS:      ", 0x4F); print_hex(r->cs,       0x4F); print("\n", 0x4F);
        print("EFLAGS:  ", 0x4F); print_hex(r->eflags,   0x4F); print("\n", 0x4F);
        print("EAX:     ", 0x4F); print_hex(r->eax,      0x4F); print("\n", 0x4F);
        print("EBX:     ", 0x4F); print_hex(r->ebx,      0x4F); print("\n", 0x4F);
        print("ECX:     ", 0x4F); print_hex(r->ecx,      0x4F); print("\n", 0x4F);
        print("EDX:     ", 0x4F); print_hex(r->edx,      0x4F); print("\n", 0x4F);
        print("ESI:     ", 0x4F); print_hex(r->esi,      0x4F); print("\n", 0x4F);
        print("EDI:     ", 0x4F); print_hex(r->edi,      0x4F); print("\n", 0x4F);
        print("EBP:     ", 0x4F); print_hex(r->ebp,      0x4F); print("\n", 0x4F);
        print("DS:      ", 0x4F); print_hex(r->ds,       0x4F); print("\n", 0x4F);
    }

    print("*** SYSTEM HALTED ***\n", 0x4F);

    for (;;) __asm__ volatile ("hlt");
}

/* =========================================================
 *  Обработчики ISR/IRQ
 * ========================================================= */
void isr_handler(struct registers *r)
{
    const char *name = (r->int_no < 32) ? exc_names[r->int_no]
                                        : "Unknown exception";
    panic(name, r);
}

void irq_handler(struct registers *r)
{
    int irq = (int)r->int_no - 32;
    if (irq < 0 || irq > 15) return;

    if (irq_handlers[irq])
        irq_handlers[irq]();

    pic_eoi((uint8_t)irq);
}