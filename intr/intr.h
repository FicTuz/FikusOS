#ifndef INTR_H
#define INTR_H

#include <stdint.h>

struct registers {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags;
};

void intr_init(void);

void pic_mask_all(void);
void pic_remap(void);
void pic_unmask(uint8_t irq);
void pic_eoi(uint8_t irq);

typedef void (*irq_fn_t)(void);
void irq_register(int irq, irq_fn_t fn);

void irq_enable(void);
void irq_disable(void);

void syscall_init(void);
void syscall_handler(struct registers *r);

void panic(const char *msg, struct registers *r);

#endif