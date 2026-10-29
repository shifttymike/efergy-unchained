#ifndef EFERGY_CONFIG_H
#define EFERGY_CONFIG_H
#include <stdint.h>
#define CONFIG_HOST_SIZE 64u
#define CONFIG_USER_SIZE 32u
#define CONFIG_PASSWORD_SIZE 64u
#define CONFIG_NAME_SIZE 32u
#define CONFIG_RECORD_V1_SIZE 197u
#define CONFIG_RECORD_V2_SIZE 199u
#define CONFIG_RECORD_SIZE 231u
#define CONFIG_VOLTAGE_DEFAULT 230u
#define CONFIG_VOLTAGE_MAX 500u
#define CONFIG_COMMIT_OFFSET 16u
#define CONFIG_SLOT_A 0x4f000ul
#define CONFIG_SLOT_B 0x50000ul
#define WEB_ASSET_BASE 0x51000ul
#define WEB_ASSET_SECTOR_SIZE 4096u
#define WEB_ASSET_SIZE 8192u
typedef struct {
    char host[CONFIG_HOST_SIZE];
    char username[CONFIG_USER_SIZE];
    char password[CONFIG_PASSWORD_SIZE];
    uint16_t port;
    uint8_t dhcp, radio_profile;
    uint8_t ip[4], mask[4], gateway[4], dns[4];
    uint16_t voltage_v;
    char device_name[CONFIG_NAME_SIZE];
} config_t;
void config_defaults(config_t *config);
uint8_t config_valid(const config_t *config);
uint8_t config_text_json(char *out,uint8_t capacity,const char *text);
uint8_t config_name_json(char *out,uint8_t capacity,const config_t *config);
/* Encode with committed=0 for flash preparation. Program byte 16 to 0xa5
 * only after the inactive slot has been verified. */
uint8_t config_encode(uint8_t record[CONFIG_RECORD_SIZE], const config_t *config,
                      uint32_t sequence, uint8_t committed);
uint8_t config_decode(const uint8_t record[CONFIG_RECORD_SIZE], config_t *config,
                      uint32_t *sequence);
uint8_t config_sequence_newer(uint32_t a, uint32_t b);
#endif
