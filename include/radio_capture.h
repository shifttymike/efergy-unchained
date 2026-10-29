#ifndef EFERGY_RADIO_CAPTURE_H
#define EFERGY_RADIO_CAPTURE_H
#include <stdint.h>
/* Fixed bank permits direct register access at the original sampling cadence. */
typedef struct {
    uint8_t data[12];
    uint8_t byte_index, bit_index, history, filtered, low_run, high_run;
    uint8_t phase, balance, samples_low, samples_high, mask, shifts;
} radio_capture_t;
extern volatile radio_capture_t raw_capture;
void radio_capture_asm(void);
#endif
