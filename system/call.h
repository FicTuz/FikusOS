#ifndef CALL_H
#define CALL_H

#include <stdint.h>

/* =========================================================
 *  FikusOS system calls — Linux i386 ABI
 *
 *  Номера syscall'ов соответствуют Linux i386.
 *  Вызов: int 0x80
 *  eax = номер, ebx/ecx/edx/esi/edi/ebp = аргументы
 *  Результат в eax (отрицательный = -errno)
 * ========================================================= */

/* --- Linux i386 syscall numbers --- */
#define SYS_EXIT          1
#define SYS_FORK          2
#define SYS_READ          3
#define SYS_WRITE         4
#define SYS_OPEN          5
#define SYS_CLOSE         6
#define SYS_WAITPID       7
#define SYS_CREAT         8
#define SYS_LINK          9
#define SYS_UNLINK       10
#define SYS_CHDIR        12
#define SYS_TIME         13
#define SYS_MKNOD        14
#define SYS_CHMOD        15
#define SYS_LSEEK        19
#define SYS_GETPID       20
#define SYS_ACCESS       33
#define SYS_BRK          45
#define SYS_IOCTL        54
#define SYS_FCNTL        55
#define SYS_DUP          41
#define SYS_DUP2         63
#define SYS_MMAP         90
#define SYS_MUNMAP       91
#define SYS_STAT        106
#define SYS_FSTAT       108
#define SYS_CLONE       120
#define SYS_UNAME       122
#define SYS_MPROTECT    125
#define SYS_GETTIMEOFDAY 78
#define SYS_EXIT_GROUP  252
#define SYS_SET_TID_ADDR 258

/* --- FikusOS extensions (не Linux) --- */
#define SYS_PRINT       300
#define SYS_PUTCHAR     301
#define SYS_CLEAR       302
#define SYS_GETCHAR     303
#define SYS_REBOOT      304
#define SYS_SLEEP       305
#define SYS_FS_CREATE   310
#define SYS_FS_LIST     311
#define SYS_FS_READ     312
#define SYS_FS_WRITE    313

/* =========================================================
 *  Ядерная часть — вызывать из isr128
 * ========================================================= */
struct registers;

void syscall_init(void);
void syscall_handler(struct registers *r);

/* =========================================================
 *  Обёртки для user-программ — Linux ABI
 * ========================================================= */

static inline int sys_write(int fd, const void *buf, uint32_t count)
{
    int r;
    __asm__ volatile ("int $0x80"
        : "=a"(r) : "a"(SYS_WRITE), "b"(fd), "c"(buf), "d"(count)
        : "memory");
    return r;
}

static inline int sys_read(int fd, void *buf, uint32_t count)
{
    int r;
    __asm__ volatile ("int $0x80"
        : "=a"(r) : "a"(SYS_READ), "b"(fd), "c"(buf), "d"(count)
        : "memory");
    return r;
}

static inline void sys_exit(int code)
{
    __asm__ volatile ("int $0x80" : : "a"(SYS_EXIT), "b"(code) : "memory");
    for (;;) { }
}

static inline int sys_open(const char *path, int flags, int mode)
{
    int r;
    __asm__ volatile ("int $0x80"
        : "=a"(r) : "a"(SYS_OPEN), "b"(path), "c"(flags), "d"(mode)
        : "memory");
    return r;
}

static inline int sys_close(int fd)
{
    int r;
    __asm__ volatile ("int $0x80"
        : "=a"(r) : "a"(SYS_CLOSE), "b"(fd)
        : "memory");
    return r;
}

static inline void *sys_brk(void *addr)
{
    void *r;
    __asm__ volatile ("int $0x80"
        : "=a"(r) : "a"(SYS_BRK), "b"(addr)
        : "memory");
    return r;
}

static inline void *sys_mmap(void *addr, uint32_t len, int prot,
                             int flags, int fd, uint32_t off)
{
    void *r;
    __asm__ volatile ("push %%ebp; push %%edi; push %%esi;"
                      "mov %5, %%esi; mov %6, %%edi; mov %7, %%ebp;"
                      "int $0x80;"
                      "pop %%esi; pop %%edi; pop %%ebp"
        : "=a"(r)
        : "a"(SYS_MMAP), "b"(addr), "c"(len), "d"(prot),
          "r"(flags), "r"(fd), "r"(off)
        : "memory");
    return r;
}

static inline int sys_munmap(void *addr, uint32_t len)
{
    int r;
    __asm__ volatile ("int $0x80"
        : "=a"(r) : "a"(SYS_MUNMAP), "b"(addr), "c"(len)
        : "memory");
    return r;
}

/* =========================================================
 *  Обёртки — FikusOS extensions
 * ========================================================= */

static inline void fsys_print(const char *s, unsigned char color)
{
    __asm__ volatile ("int $0x80"
        : : "a"(SYS_PRINT), "b"(s), "c"(color) : "memory");
}

static inline void fsys_putchar(char c, unsigned char color)
{
    __asm__ volatile ("int $0x80"
        : : "a"(SYS_PUTCHAR), "b"(c), "c"(color) : "memory");
}

static inline void fsys_clear(unsigned char color)
{
    __asm__ volatile ("int $0x80" : : "a"(SYS_CLEAR), "b"(color) : "memory");
}

static inline char fsys_getchar(void)
{
    char c;
    __asm__ volatile ("int $0x80" : "=a"(c) : "a"(SYS_GETCHAR) : "memory");
    return c;
}

static inline void fsys_reboot(void)
{
    __asm__ volatile ("int $0x80" : : "a"(SYS_REBOOT) : "memory");
}

static inline void fsys_sleep(uint32_t ms)
{
    __asm__ volatile ("int $0x80" : : "a"(SYS_SLEEP), "b"(ms) : "memory");
}

#endif