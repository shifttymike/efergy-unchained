#ifndef EFERGY_NETWORK_H
#define EFERGY_NETWORK_H
#include <stdint.h>
#define EFERGY_DHCP_HOST_NAME "efergyhub"
extern uint8_t network_state,network_fallback;
void network_init(void);
void network_task(void);
uint8_t network_config_pending(void);
#endif
