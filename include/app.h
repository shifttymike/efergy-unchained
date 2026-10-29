#ifndef EFERGY_APP_H
#define EFERGY_APP_H
#include <stdint.h>
extern uint8_t app_network_ready,app_reboot_requested;
uint16_t app_format_status(char *out,uint16_t capacity);
void http_server_task(void);
#endif
