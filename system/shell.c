#include "../video/video.h"
#include "../time/time.h"
#include "../input/input.h"
#include "../string/str.h"
#include "../fs/ramfs.h"
#include "../intr/intr.h"
#include "cpuinfo.h"
#include "meminfo.h"

static const char passw_root[] =
    "\x52\x4F\x4F\x54\x4E\x4F\x54\x52\x4F\x4F\x54"
    "\x42\x55\x54\x52\x4F\x4F\x54\x49\x53\x41\x52"
    "\x4F\x4F\x54";
char comd[65];
char *argv[8];
int   argc;
int rgts = -1;
char pasw[100];
char name[64];
const char *kn = "Cacpys";
const char *kw = "Cacpys-0.1.0-2610";

/* =========================================================
 *  Перезагрузка
 * ========================================================= */
void reboot(void) {
    __asm__ volatile ("cli");
    unsigned char status;
    do {
        __asm__ volatile ("inb %1, %0" : "=a"(status) : "d"((unsigned short)0x64));
    } while (status & 0x02);
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"((unsigned char)0xFE),
          "d"((unsigned short)0x64)
    );
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"((unsigned char)0x0E),
          "d"((unsigned short)0xCF9)
    );
    for (;;) __asm__ volatile ("hlt");
}

/* =========================================================
 *  Команды
 * ========================================================= */
static void cmd_help(void) {
    print("help    - list of commands\n", 0x06);
    print("clear   - clear screen\n", 0x06);
    print("reboot  - restart\n", 0x06);
    print("whoami  - show info\n", 0x06);
    print("time    - show RTC time\n", 0x06);
    print("cpu     - cpu info\n", 0x06);
    print("mem     - memory info\n", 0x06);
    print("ls      - list dir\n", 0x06);
    print("touch   - create file\n", 0x06);
    print("rm      - delete file\n", 0x06);
    print("cat     - show file\n", 0x06);
    print("echo    - print text\n", 0x06);
}

static void cmd_clear(void) {
    video_clear(0x00);
}

static void cmd_time(void) {
    struct rtc_time t;
    rtc_read(&t);
    time_print(&t, 0x0F);
    print("\n", 0x0F);
}

static void cmd_whoami(void) {
    print("username: ", 0x0F);
    print(name, 0x0F);
    print("\nkernel ver: ", 0x0F);
    print(kw, 0x0F);
    print("\nuser rights: ", 0x0F);
    print_int(rgts, 0x0F);
    if (rgts == 0)  print(" (system rights)", 0x0F);
    if (rgts == -1) print(" (full rights)", 0x0F);
    if (rgts > 1 && rgts < 15) print(" (based rights)", 0x0F);
    print("\n", 0x0F);
}

static void cmd_cpu(void) {
    fcpuid();
}

static void cmd_mem(void) {
    fmeminfo();
}

static void cmd_ls(void) {
    fs_list();
}

static void cmd_touch(void)
{
    if (argc < 2) {
        print("usage: touch <filename>\n", 0x0C);
        return;
    }

    int r = fs_create(argv[1]);
    if (r == -3) {
        print("touch: already exists\n", 0x0C);
    } else if (r != 0) {
        print("touch: cannot create\n", 0x0C);
    }
}

static void cmd_rm(void)
{
    if (argc < 2) {
        print("usage: rm <filename>\n", 0x0C);
        return;
    }

    int r = fs_delete(argv[1]);
    if (r != 0) {
        print("rm: no such file\n", 0x0C);
    }
}

static void cmd_cat(void)
{
    if (argc < 2) {
        print("usage: cat <filename>\n", 0x0C);
        return;
    }

    static char buf[1024];
    int n = fs_read(argv[1], buf, 1023);
    if (n < 0) {
        print("cat: no such file\n", 0x0C);
        return;
    }
    buf[n] = 0;
    print(buf, 0x0F);
    print("\n", 0x0F);
}

static void cmd_echo(void)
{
    int redirect = -1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], ">") == 0) { redirect = i; break; }
    }

    static char buf[512];
    int n = 0;
    int end = (redirect < 0) ? argc : redirect;

    for (int i = 1; i < end; i++) {
        const char *p = argv[i];
        while (*p && n < 510) buf[n++] = *p++;
        if (i < end - 1 && n < 510) buf[n++] = ' ';
    }
    buf[n] = 0;

    if (redirect >= 0) {
        if (redirect + 1 >= argc) {
            print("echo: missing filename\n", 0x0C);
            return;
        }
        int r = fs_write(argv[redirect + 1], buf, n);
        if (r != 0) print("echo: write failed\n", 0x0C);
        return;
    }

    print(buf, 0x0F);
    print("\n", 0x0F);
}

/* =========================================================
 *  Диспетчер команд
 * ========================================================= */
static void run(void) {
    if (argc == 0) return;

    if (strcmp(argv[0], "help")   == 0) { cmd_help();   return; }
    if (strcmp(argv[0], "clear")  == 0) { cmd_clear();  return; }
    if (strcmp(argv[0], "reboot") == 0) { reboot();     return; }
    if (strcmp(argv[0], "whoami") == 0) { cmd_whoami(); return; }
    if (strcmp(argv[0], "time")   == 0) { cmd_time();   return; }
    if (strcmp(argv[0], "cpu")    == 0) { cmd_cpu();    return; }
    if (strcmp(argv[0], "mem")    == 0) { cmd_mem();    return; }
    if (strcmp(argv[0], "ls")     == 0) { cmd_ls();     return; }
    if (strcmp(argv[0], "touch")  == 0) { cmd_touch();  return; }
    if (strcmp(argv[0], "rm")     == 0) { cmd_rm();     return; }
    if (strcmp(argv[0], "cat")    == 0) { cmd_cat();    return; }
    if (strcmp(argv[0], "echo")   == 0) { cmd_echo();   return; }

    print("unknown command: ", 0x0C);
    print(argv[0], 0x0C);
    putchar('\n', 0x0C);
}

/* =========================================================
 *  Основной цикл терминала
 * ========================================================= */
void gotot(void) {
    for (;;) {
        print(name, 0x02);
        print("@FikusOS ~$ ", 0x02);
        read_line(comd, 64, 0x0F);

        argc = 0;
        char *p = comd;

        while (*p && argc < 8) {
            while (*p == ' ' || *p == '\t') p++;
            if (!*p) break;
            argv[argc++] = p;
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) *p++ = '\0';
        }

        run();
    }
}

/* =========================================================
 *  Логин
 * ========================================================= */
void console(void) {
    print("login: ", 0x02);
    read_line(name, 64, 0x0F);

    print("password: ", 0x02);
    read_line(pasw, 100, 0x0F);

    if (strcmp(name, "user") == 0 && strcmp(pasw, "") == 0) {
        rgts = 10;
        gotot();
    }
    if (strcmp(name, "fictuz") == 0 && strcmp(pasw, "8220282202") == 0) {
        rgts = 1;
        gotot();
    }
    if (strcmp(name, "root") == 0 && strcmp(pasw, passw_root) == 0) {
        rgts = 0;
        gotot();
    }
    if (strcmp(name, "system") == 0 && strcmp(pasw, "SYSTEMSUPERPASSWORDFORSYSTEMRIGHTS") == 0) {
        rgts = -1;
        gotot();
    }

    print("Login failed.\n", 0x0C);
}