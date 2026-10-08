#include "ramfs.h"
#include "../string/str.h"
#include "../video/video.h"

#define MAX_FILES   64
#define NAME_LEN    32
#define DISK_SIZE   1048576

struct fs_entry {
    char     filename[NAME_LEN];
    uint32_t offset;
    uint32_t size;
    uint8_t  used;
};

static struct fs_entry entries[MAX_FILES];
static char            disk[DISK_SIZE];
static uint32_t        disk_used = 0;

/* =========================================================
 *  Поиск
 * ========================================================= */
int fs_find(const char *filename)
{
    for (int i = 0; i < MAX_FILES; i++) {
        if (entries[i].used && strcmp(entries[i].filename, filename) == 0)
            return i;
    }
    return -1;
}

/* =========================================================
 *  Создать пустой файл
 * ========================================================= */
int fs_create(const char *filename)
{
    if (fs_find(filename) >= 0) return FS_ERR_EXIST;

    int i = -1;
    for (int j = 0; j < MAX_FILES; j++) {
        if (!entries[j].used) { i = j; break; }
    }
    if (i < 0) return FS_ERR_NOSPC;

    strncpy(entries[i].filename, filename, NAME_LEN - 1);
    entries[i].filename[NAME_LEN - 1] = 0;
    entries[i].offset = 0;
    entries[i].size   = 0;
    entries[i].used   = 1;

    return FS_OK;
}

/* =========================================================
 *  Записать в файл — offset выбирает сама ФС
 * ========================================================= */
int fs_write(const char *filename, const void *data, uint32_t size)
{
    int i = fs_find(filename);
    if (i < 0) return FS_ERR_NOENT;

    uint32_t offset;

    if (entries[i].size >= size) {
        /* новый размер влезает в старый слот — пишем туда же */
        offset = entries[i].offset;
    } else {
        /* нужен новый блок в конце пула */
        offset = disk_used;
        if (offset + size > DISK_SIZE) return FS_ERR_NOSPC;
        disk_used += size;
    }

    memcpy(disk + offset, data, size);
    entries[i].offset = offset;
    entries[i].size   = size;

    return FS_OK;
}

/* =========================================================
 *  Прочитать
 * ========================================================= */
int fs_read(const char *filename, void *buf, uint32_t max)
{
    int i = fs_find(filename);
    if (i < 0) return FS_ERR_NOENT;

    uint32_t n = entries[i].size;
    if (n > max) n = max;

    memcpy(buf, disk + entries[i].offset, n);
    return (int)n;
}

/* =========================================================
 *  Удалить
 * ========================================================= */
int fs_delete(const char *filename)
{
    int i = fs_find(filename);
    if (i < 0) return FS_ERR_NOENT;

    entries[i].used = 0;
    return FS_OK;
}

/* =========================================================
 *  Список
 * ========================================================= */
void fs_list(void)
{
    int any = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (!entries[i].used) continue;
        any = 1;
        print(entries[i].filename, 0x0F);
        print("  ", 0x08);
        print_int((int)entries[i].size, 0x0A);
        print("\n", 0x0F);
    }
    if (!any) print("(empty)\n", 0x08);
}

/* =========================================================
 *  Init
 * ========================================================= */
void fs_init(void)
{
    for (int i = 0; i < MAX_FILES; i++)
        entries[i].used = 0;
    disk_used = 0;
}