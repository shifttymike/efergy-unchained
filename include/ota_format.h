#ifndef EFERGY_OTA_FORMAT_H
#define EFERGY_OTA_FORMAT_H
#include <stdint.h>
#define OTA_APP_BASE 0x400ul
#define OTA_BOOT_BASE 0x1cc00ul
#define OTA_APP_SIZE (OTA_BOOT_BASE-OTA_APP_BASE)
#define OTA_SLOT_SIZE 0x1d000ul
#define OTA_HEADER_SIZE 64u
#define OTA_JOURNAL_A 0x3a000ul
#define OTA_JOURNAL_B 0x3b000ul
#define OTA_MODEL 0x18676069ul
#define OTA_NONE 255u
#define OTA_CONFIRMED 0u
#define OTA_PENDING 1u
#define OTA_TRIAL 2u
#define OTA_RECORD_SIZE 32u
uint32_t ota_crc(uint32_t crc,const uint8_t *data,uint16_t length);
uint32_t ota_u32(const uint8_t *p);
void ota_put32(uint8_t *p,uint32_t v);
uint8_t ota_header_valid(const uint8_t *p);
uint8_t ota_record_valid(const uint8_t *p);
void ota_record(uint8_t *p,uint32_t sequence,uint8_t active,uint8_t candidate,uint8_t phase);
#endif
