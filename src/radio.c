#include <string.h>
#include "HardwareProfile.h"
#include "radio.h"
#include "radio_capture.h"
#include "radio_profiles.h"

volatile uint32_t radio_captures;
volatile uint16_t radio_overruns;
uint8_t radio_version;
static radio_frame_t captured;
volatile radio_capture_t raw_capture __at(0xe00);
static volatile uint8_t pending;
static uint8_t active_profile;
static uint8_t rearm_configured;

static void configure_profile(void)
{
    uint8_t i;
    const radio_register_t *table=active_profile?profile_b:profile_a;
    uint8_t count=active_profile?sizeof(profile_b)/sizeof(profile_b[0]):sizeof(profile_a)/sizeof(profile_a[0]);
    for(i=0;i<count;++i) radio_write(table[i].reg,table[i].value);
}

static uint8_t transfer(uint8_t value)
{
    uint8_t i, received = 0;
    for (i=0; i<8; ++i) {
        LATEbits.LATE4 = (value & 0x80u) != 0;
        LATEbits.LATE3 = 0; Nop();
        LATEbits.LATE3 = 1;
        received = (uint8_t)((received << 1) | PORTDbits.RD0);
        value <<= 1;
    }
    LATEbits.LATE3 = 0;
    return received;
}

uint8_t radio_read(uint8_t reg)
{
    uint8_t value;
    LATEbits.LATE3 = 0; LATEbits.LATE5 = 0;
    transfer(reg & 0x7fu); value = transfer(0);
    LATEbits.LATE5 = 1; LATEbits.LATE4 = 0;
    return value;
}

void radio_write(uint8_t reg, uint8_t value)
{
    LATEbits.LATE3 = 0; LATEbits.LATE5 = 0;
    transfer(reg | 0x80u); transfer(value);
    LATEbits.LATE5 = 1; LATEbits.LATE4 = 0;
}

void radio_init(uint8_t profile)
{
    uint8_t ignored;
    active_profile = profile ? 1 : 0;
    INTCONbits.RBIE = 0; INTCON3bits.INT2IE = 0;
    LATEbits.LATE1 = 1; _delay(10417); LATEbits.LATE1 = 0; _delay(104167);
    configure_profile();
    radio_version=radio_read(0x10);
    pending = 0;
    rearm_configured=1;
    ignored = PORTB; (void)ignored;
    INTCONbits.RBIF = 0; INTCON3bits.INT2IF = 0;
    INTCON2bits.RBIP = 1; INTCON3bits.INT2IP = 1;
    /* radio_task performs the original RB4-low wait before entering RX. */
}

void radio_interrupt(void)
{
    uint8_t ignored;
    if (!((INTCONbits.RBIE && INTCONbits.RBIF) ||
          (INTCON3bits.INT2IE && INTCON3bits.INT2IF))) return;
    INTCONbits.RBIE = 0; INTCON3bits.INT2IE = 0;
    ignored = PORTB; (void)ignored;
    INTCONbits.RBIF = 0; INTCON3bits.INT2IF = 0;
    if (pending) { ++radio_overruns; return; }
    memset(&captured,0,sizeof(captured));
    captured.profile = active_profile;
    /* Read the receiver's preamble RSSI at the receive interrupt, before the
     * bounded sampler can continue into noise after the transmitter stops.
     * RFM69W section 3.4.9 / RegRssiValue: RSSI = -raw/2 dBm. */
    captured.rssi_raw = radio_read(0x24);
    if (active_profile) {
        memset((void *)&raw_capture,0,sizeof(raw_capture));
        T3CON = 0x80;
        TMR3H = 0xff; TMR3L = 0xa0; PIR2bits.TMR3IF = 0;
        T3CONbits.TMR3ON = 1;
        radio_capture_asm();
        T3CONbits.TMR3ON = 0;
        memcpy(captured.data,(const void *)raw_capture.data,12);
        captured.length = raw_capture.byte_index;
        captured.bits = (uint8_t)(raw_capture.byte_index*8u + raw_capture.bit_index);
    } else {
        /* Original FIFO path is gated by RB2 and bounded at 16 bytes. */
        while (PORTBbits.RB2 && captured.length < RADIO_FRAME_MAX)
            captured.data[captured.length++] = radio_read(0);
        captured.bits = (uint8_t)(captured.length*8u);
    }
    radio_write(1,0x04);
    ++radio_captures;
    pending = 1;
}

uint8_t radio_take(radio_frame_t *frame)
{
    if (!pending) return 0;
    memcpy(frame,&captured,sizeof(*frame));
    pending = 0;
    return 1;
}

void radio_task(void)
{
    uint8_t ignored;
    if (pending || INTCONbits.RBIE || INTCON3bits.INT2IE) return;
    /* Original function_118 reapplies the full profile on each receive arm and
     * waits for RB4 low before switching to RX. Preserve that ordering. */
    if(!rearm_configured) {configure_profile();rearm_configured=1;}
    if(active_profile && PORTBbits.RB4) return;
    radio_write(1,0x10);
    ignored = PORTB; (void)ignored;
    INTCONbits.RBIF = 0; INTCON3bits.INT2IF = 0;
    if (active_profile) INTCONbits.RBIE = 1;
    else INTCON3bits.INT2IE = 1;
    rearm_configured=0;
}
