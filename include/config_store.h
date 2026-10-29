#ifndef EFERGY_CONFIG_STORE_H
#define EFERGY_CONFIG_STORE_H
#include "config.h"
#include "app_workspace.h"
/* Enabled only after a fresh full flash backup and sector inspection. */
#ifndef CONFIG_WRITES_ENABLED
#define CONFIG_WRITES_ENABLED 1
#endif
extern config_t device_config;
/* Main-loop scratch shared by boot/commit decoding and HTTP patches. HTTP may
 * use it only while the store is idle; the async save owns an encoded record. */
#define config_scratch (app_workspace.config)
extern uint32_t config_sequence;
extern uint8_t config_store_state,config_store_error,config_generation;
void config_store_init(void);
uint8_t config_store_begin(const config_t *candidate);
void config_store_task(void);
uint8_t config_store_busy(void);
#endif
