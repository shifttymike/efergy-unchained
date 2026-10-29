#ifndef EFERGY_WEB_ASSETS_H
#define EFERGY_WEB_ASSETS_H
#include <stdint.h>
/* 0 ready, 1 scan, 2 erase wait, 3 program wait, 4 verify, 5 commit wait, 255 failed. */
extern uint8_t web_assets_state;
void web_assets_init(void);
void web_assets_task(void);
uint8_t web_assets_ready(void);
uint8_t web_assets_busy(void);
uint16_t web_assets_length(void);
#endif
