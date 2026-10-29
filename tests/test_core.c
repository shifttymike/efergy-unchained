#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "radio.h"
static void run(radio_sampler_t *s, uint8_t level, unsigned ticks)
{
    while (ticks--) (void)radio_sampler_tick(s, level);
}
int main(void)
{
    uint8_t small[] = {0x0c,0xc0,0x55,0x60,0x4a,0x41,0x03,0x0f};
    uint8_t large[] = {0x0c,0xc0,0x55,0x60,0x5c,0x94,0x08,0x79};
    uint8_t inverse[8];
    reading_t reading;
    radio_sampler_t sampler;
    radio_frame_t capture={0};
    unsigned i, bit;
    assert(e2_decode(small,64,&reading));
    assert(reading.transmitter_id == 0x55c0);
    assert(reading.legacy_id == 0x0cc055);
    assert(reading.current_mantissa == 0x4a41 && reading.current_divisor_shift == 12);
    assert(reading.interval_s == 18 && reading.battery_ok && !reading.learn);
    assert(e2_decode(large,65,&reading));
    assert(reading.current_mantissa == 0x5c94 && reading.current_divisor_shift == 7);
    for(i=0; i<8; ++i) inverse[i] = small[i] ^ 255;
    assert(e2_decode(inverse,64,&reading));
    small[7] ^= 1; assert(!e2_decode(small,64,&reading)); small[7] ^= 1;
    assert(!e2_decode(small,63,&reading)); assert(!e2_decode(small,66,&reading));
    memset(inverse,0,sizeof(inverse)); assert(!e2_decode(inverse,64,&reading));
    /* Captured on this board: valid inverted first 64 bits, six trailing bits. */
    {
        static const uint8_t live[]={0xf3,0x5e,0x7a,0xbf,0x87,0x6d,0xfd,0x81};
        memcpy(capture.data,live,8);capture.length=8;capture.bits=70;capture.profile=1;capture.rssi_raw=169;
        assert(e2_decode_capture(&capture,&reading));
        assert(reading.legacy_id==827781 && reading.interval_s==6 && reading.rssi_raw==169);
        assert(reading.current_mantissa==0x7892 && reading.current_divisor_shift==13);
        capture.data[7]^=1;assert(!e2_decode_capture(&capture,&reading));
    }
    /* Synthetic sampled waveform exercises the recovered preamble/filter/PWM
     * states. It is a software regression fixture, not a captured RF waveform. */
    radio_sampler_reset(&sampler);
    run(&sampler,0,105); run(&sampler,1,55);
    for (i=0; i<8; ++i) for(bit=0; bit<8; ++bit) {
        uint8_t one = small[i] & (0x80u >> bit);
        run(&sampler,0,one ? 15 : 7); run(&sampler,1,one ? 7 : 15);
    }
    run(&sampler,0,30);
    assert(sampler.byte_index == 8);
    assert(memcmp(sampler.data,small,8) == 0);
    radio_sampler_reset(&sampler);
    for(i=0; i<5001; ++i) (void)radio_sampler_tick(&sampler,i & 1);
    assert(sampler.samples == 5000);
    puts("core: decoder vectors, inversion, checksum/length rejection, sampled PWM and capture bound passed");
    return 0;
}
