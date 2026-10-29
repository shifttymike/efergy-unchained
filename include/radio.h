#ifndef EFERGY_RADIO_H
#define EFERGY_RADIO_H
#include <stdint.h>
#define RADIO_FRAME_MAX 16u
typedef struct {
    uint8_t data[RADIO_FRAME_MAX];
    uint8_t length;
    uint8_t bits;
    uint8_t rssi_raw;
    uint8_t profile;
} radio_frame_t;
typedef struct {
    uint32_t legacy_id;
    uint16_t transmitter_id;
    uint16_t current_mantissa;
    uint8_t current_divisor_shift;
    uint8_t interval_s;
    uint8_t battery_ok;
    uint8_t learn;
    uint8_t rssi_raw;
} reading_t;
/* Independent implementation of the published e2 classic field specification. */
uint8_t e2_decode(const uint8_t *data, uint8_t bits, reading_t *out);
uint8_t e2_decode_capture(const radio_frame_t *frame, reading_t *out);
typedef struct {
    uint8_t data[12];
    uint8_t history, high_run, low_run, phase, bit_index, byte_index;
    int8_t balance;
    uint16_t samples;
} radio_sampler_t;
void radio_sampler_reset(radio_sampler_t *sampler);
/* Returns nonzero when capture ends. One call per original Timer3 sample. */
uint8_t radio_sampler_tick(radio_sampler_t *sampler, uint8_t level);
void radio_init(uint8_t profile);
void radio_task(void);
uint8_t radio_take(radio_frame_t *frame);
void radio_interrupt(void);
uint8_t radio_read(uint8_t reg);
void radio_write(uint8_t reg, uint8_t value);
extern volatile uint16_t radio_captures, radio_overruns;
extern uint8_t radio_version;
#endif
