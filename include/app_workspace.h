#ifndef EFERGY_APP_WORKSPACE_H
#define EFERGY_APP_WORKSPACE_H
#include "config.h"
#include "mqtt_codec.h"
/* Cooperative main-loop workspace. MQTT constructs and copies a packet into
 * Ethernet RAM before returning; no pending transmission borrows this buffer.
 * Boot/commit decoding and HTTP patches may therefore use the config member
 * between MQTT calls. Radio/Tick interrupts must never use this workspace. */
typedef union {uint8_t packet[MQTT_PACKET_MAX];config_t config;} app_workspace_t;
extern app_workspace_t app_workspace;
#endif
