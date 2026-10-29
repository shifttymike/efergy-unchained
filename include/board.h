#ifndef EFERGY_BOARD_H
#define EFERGY_BOARD_H
#include <stdint.h>
void board_init(void);
extern uint8_t board_network_activity_pending;
void board_radio_activity(void);
void board_leds_task(void);
uint8_t board_read_mac(uint8_t mac[6]);
uint8_t flash_read(uint32_t address, uint8_t *data, uint16_t count);
uint32_t flash_id(void);
uint8_t flash_status(uint8_t *status);
uint8_t flash_erase_config_sector(uint32_t address);
uint8_t flash_program_config_byte(uint32_t address,uint8_t value);
uint8_t flash_erase_asset_sector(uint8_t sector);
uint8_t flash_program_asset_byte(uint16_t offset,uint8_t value);
#endif
