#ifndef MEMINFO_H
#define MEMINFO_H

#ifdef __cplusplus
extern "C" {
#endif

void meminfo(void);   /* brief overview */
void fmeminfo(void);  /* full dump */
void hmeminfo(void);  /* hardware: E820 map + page size */

#ifdef __cplusplus
}
#endif

#endif