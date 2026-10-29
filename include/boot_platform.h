#ifndef EFERGY_BOOT_PLATFORM_H
#define EFERGY_BOOT_PLATFORM_H
#include <stdint.h>
void boot_pic_read(uint32_t address,uint8_t *p,uint8_t n);
uint8_t boot_pic_row(uint32_t address,const uint8_t *p);
void boot_reboot(void);
uint8_t boot_run(void);
#ifdef BOOT_BENCH_TRACE
void boot_trace(uint8_t stage,uint8_t value);
#define BOOT_TRACE(s,v) boot_trace(s,v)
#else
#define BOOT_TRACE(s,v) ((void)0)
#endif
#endif
