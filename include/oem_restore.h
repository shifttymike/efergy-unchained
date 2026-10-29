#ifndef EFERGY_OEM_RESTORE_H
#define EFERGY_OEM_RESTORE_H
#include <stdint.h>
#ifdef OEM_RESTORE_BUILD
void oem_restore_task(void);
uint8_t oem_restore_busy(void);
uint8_t oem_restore_chunk_pending(void);
uint8_t oem_restore_failed(void);
void oem_restore_abort(void);
uint16_t oem_restore_begin(const uint8_t *p,uint16_t n);
uint16_t oem_restore_chunk(const uint8_t *p,uint16_t n);
uint16_t oem_restore_commit(void);
uint16_t oem_restore_status(char *out,uint16_t capacity);
uint8_t restore_flash_program(uint32_t a,uint8_t v);
uint8_t restore_flash_erase(uint32_t a);
#else
#define oem_restore_busy() 0
#endif
#endif
