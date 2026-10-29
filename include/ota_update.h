#ifndef EFERGY_OTA_UPDATE_H
#define EFERGY_OTA_UPDATE_H
#include <stdint.h>
void ota_update_init(void);
void ota_update_task(void);
void ota_update_abort(void);
uint8_t ota_update_busy(void);
uint16_t ota_update_begin(const uint8_t *header,uint16_t n);
uint16_t ota_update_chunk(const uint8_t *body,uint16_t n);
uint16_t ota_update_commit(void);
uint16_t ota_update_rollback(void);
uint16_t ota_update_status(char *out,uint16_t capacity);
uint8_t ota_update_chunk_pending(void);
uint8_t ota_update_failed(void);
#endif
