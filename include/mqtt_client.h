#ifndef EFERGY_MQTT_CLIENT_H
#define EFERGY_MQTT_CLIENT_H
#include "radio.h"
extern uint8_t mqtt_state,mqtt_error,mqtt_sensor_count;
extern uint8_t mqtt_tcp_phase;
extern uint32_t mqtt_resolved_ip;
extern uint16_t mqtt_reconnects,mqtt_publications,mqtt_dropped_sensors;
void mqtt_init(void);
void mqtt_reconfigure(void);
void mqtt_reading(const reading_t *reading);
void mqtt_task(uint8_t network_ready);
#endif
