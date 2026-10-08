#ifndef RAMFS_H
#define RAMFS_H

#include <stdint.h>

#define FS_OK           0
#define FS_ERR_NOENT   -1
#define FS_ERR_NOSPC   -2
#define FS_ERR_EXIST   -3
#define FS_ERR_PARAM   -4

void fs_init(void);
int  fs_create(const char *filename);
int  fs_find(const char *filename);
int  fs_write(const char *filename, const void *data, uint32_t size);
int  fs_read(const char *filename, void *buf, uint32_t max);
int  fs_delete(const char *filename);
void fs_list(void);

#endif