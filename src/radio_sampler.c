#include <string.h>
#include "radio.h"

void radio_sampler_reset(radio_sampler_t *s)
{
    memset(s, 0, sizeof(*s));
}

uint8_t radio_sampler_tick(radio_sampler_t *s, uint8_t level)
{
    uint8_t filtered;
    if (s->phase >= 6 || s->byte_index >= 12 || s->samples >= 5000) return 1;
    ++s->samples;
    s->history = (uint8_t)((s->history << 1) | (level != 0));
    filtered = s->history & 7u;
    if (filtered == 7 || filtered == 6 || filtered == 5 || filtered == 3) {
        ++s->high_run;
        s->low_run = 0;
    } else {
        ++s->low_run;
        s->high_run = 0;
    }
    switch (s->phase) {
    case 0:
        if (s->low_run >= 100) { s->phase = 1; s->low_run = 0; }
        break;
    case 1:
        if (s->high_run) s->phase = 2;
        break;
    case 2:
        if (s->low_run) s->phase = 0;
        else if (s->high_run >= 20) s->phase = 3;
        break;
    case 3:
        if (s->low_run) {
            s->phase = 4; s->balance = -1;
            s->byte_index = 0; s->bit_index = 0;
        }
        break;
    case 4:
        if (s->low_run > 20) s->phase = 6;
        else if (s->low_run) --s->balance;
        else { ++s->balance; s->phase = 5; }
        break;
    case 5:
        if (s->high_run > 20) s->phase = 6;
        else if (s->high_run) ++s->balance;
        else {
            if (s->balance < 0) s->data[s->byte_index] |= (uint8_t)(0x80u >> s->bit_index);
            if (++s->bit_index == 8) { s->bit_index = 0; ++s->byte_index; }
            s->balance = -1; s->phase = 4;
        }
        break;
    default: break;
    }
    return s->phase >= 6 || s->byte_index >= 12 || s->samples >= 5000;
}
