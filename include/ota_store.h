#ifndef EFERGY_OTA_STORE_H
#define EFERGY_OTA_STORE_H
#include "ota_format.h"
/* Shared by the application and immutable bootloader. All writes are range checked. */
uint8_t ota_flash_read(uint32_t address,uint8_t *p,uint16_t n);
uint8_t ota_flash_program(uint32_t address,uint8_t value);
uint8_t ota_flash_erase(uint32_t address);
uint8_t ota_flash_ready(void);
uint8_t ota_flash_wait(void);
uint8_t ota_find_record(uint8_t *record,uint32_t *address);
uint8_t ota_next_record(uint32_t last,uint32_t *next,uint8_t *erase);
uint8_t ota_append_sync(uint8_t *record,uint32_t last);
uint8_t ota_slot_valid(uint8_t slot,uint8_t *scratch);
#endif
