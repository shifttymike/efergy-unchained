#include "HardwareProfile.h"
#include "board.h"
#include "config.h"
#include "TCPIP Stack/Tick.h"

uint8_t board_network_activity_pending;
static uint8_t radio_activity_pending;
static DWORD radio_led_started,network_led_started;

#pragma config WDT=OFF, STVR=ON, XINST=OFF, DEBUG=OFF, CP0=OFF
#pragma config FOSC=HSPLL, FOSC2=ON, FCMEN=OFF, IESO=OFF, ETHLED=ON
#pragma config WDTPS=8192

void board_init(void)
{
    INTCON = 0;
    ADCON1 = 0x0f;
    CMCON = 7;
    OSCTUNE = 0x40;
    /* Original status outputs at 0x00c112-0x00c118. Active-high LEDs:
     * yellow RF3 (radio), green RD1 (Ethernet). Set latches before outputs. */
    LATFbits.LATF3=0;LATDbits.LATD1=0;
    TRISFbits.TRISF3=0;TRISDbits.TRISD1=0;
    /* Original startup: RE1 RFM reset; RF CS/SCK/MOSI, RD0 MISO. */
    LATEbits.LATE5 = 1;
    LATEbits.LATE3 = 0;
    LATEbits.LATE4 = 0;
    LATEbits.LATE1 = 0;
    TRISEbits.TRISE1 = 0;
    TRISEbits.TRISE3 = 0;
    TRISEbits.TRISE4 = 0;
    TRISEbits.TRISE5 = 0;
    TRISDbits.TRISD0 = 1;
    TRISB |= 0x3f;
    /* Flash CS: LATC1 brackets SSP1BUF command/address/data transfers
     * at original 0x017362-0x0173d2. RD1/RF3 are status outputs. */
    LATCbits.LATC1 = 1; TRISCbits.TRISC1 = 0;
    LATCbits.LATC0 = 1; TRISCbits.TRISC0 = 0;
    LATCbits.LATC3 = 0; TRISCbits.TRISC3 = 0;
    TRISCbits.TRISC4 = 1; TRISCbits.TRISC5 = 0;
    SSP1STAT = 0x40; SSP1CON1 = 0x21;
    RCONbits.IPEN = 1;
}

void board_radio_activity(void) {radio_activity_pending=1;}

/* Stretch short events into visible 125ms pulses without delaying reception.
 * MAC hooks only set a flag; all timing/GPIO work stays in the main loop. */
void board_leds_task(void)
{
    DWORD now=TickGet();
    if(radio_activity_pending) {
        radio_activity_pending=0;radio_led_started=now;LATFbits.LATF3=1;
    }
    if(board_network_activity_pending) {
        board_network_activity_pending=0;network_led_started=now;LATDbits.LATD1=1;
    }
    if(LATFbits.LATF3 && (DWORD)(now-radio_led_started)>=(DWORD)(TICK_SECOND/8u))LATFbits.LATF3=0;
    if(LATDbits.LATD1 && (DWORD)(now-network_led_started)>=(DWORD)(TICK_SECOND/8u))LATDbits.LATD1=0;
}

/* Original 0x019eac-0x019fd8: SDA RA4 (TRIS open drain), SCL RA5.
 * 0x019d88 selects the read-only EUI-48 range starting at 0xfa. */
static void i2c_delay(void) {_delay(105);}
static void i2c_start(void)
{
    TRISAbits.TRISA4=1;LATAbits.LATA5=1;i2c_delay();
    TRISAbits.TRISA4=0;i2c_delay();LATAbits.LATA5=0;
}
static void i2c_stop(void)
{
    LATAbits.LATA5=0;TRISAbits.TRISA4=0;i2c_delay();
    LATAbits.LATA5=1;i2c_delay();TRISAbits.TRISA4=1;i2c_delay();
}
static uint8_t i2c_write(uint8_t value)
{
    uint8_t i,ack;
    for(i=0;i<8;++i) {
        TRISAbits.TRISA4=(value&0x80u)!=0;i2c_delay();
        LATAbits.LATA5=1;i2c_delay();LATAbits.LATA5=0;value<<=1;
    }
    TRISAbits.TRISA4=1;i2c_delay();LATAbits.LATA5=1;i2c_delay();
    ack=!PORTAbits.RA4;LATAbits.LATA5=0;return ack;
}
static uint8_t i2c_read(uint8_t ack)
{
    uint8_t i,value=0;
    TRISAbits.TRISA4=1;
    for(i=0;i<8;++i) {
        i2c_delay();LATAbits.LATA5=1;i2c_delay();
        value=(uint8_t)((value<<1)|PORTAbits.RA4);LATAbits.LATA5=0;
    }
    TRISAbits.TRISA4=!ack;i2c_delay();LATAbits.LATA5=1;i2c_delay();
    LATAbits.LATA5=0;TRISAbits.TRISA4=1;return value;
}
uint8_t board_read_mac(uint8_t mac[6])
{
    /* Emergency locally administered address, used only on EEPROM failure.
     * Stock devices normally retain their own factory MAC from the EEPROM. */
    static const uint8_t fallback_mac[6] = {0x02,0xef,0x67,0x60,0x00,0x01};
    uint8_t i,nonzero=0,not_ff=0;
    LATAbits.LATA4=0;TRISAbits.TRISA4=1;
    LATAbits.LATA5=1;TRISAbits.TRISA5=0;
    i2c_start();
    if(!i2c_write(0xa0) || !i2c_write(0xfa)) {i2c_stop();goto fallback;}
    i2c_start();
    if(!i2c_write(0xa1)) {i2c_stop();goto fallback;}
    for(i=0;i<6;++i) {mac[i]=i2c_read(i<5);nonzero|=mac[i];not_ff|=(uint8_t)~mac[i];}
    i2c_stop();
    if(nonzero && not_ff && !(mac[0]&1u)) return 1;
fallback:
    for (i=0; i<6; ++i) mac[i] = fallback_mac[i];
    return 0;
}

static uint8_t spi_fault;
static uint8_t spi(uint8_t value)
{
    uint16_t timeout = 65535;
    PIR1bits.SSP1IF = 0; SSP1BUF = value;
    while (!PIR1bits.SSP1IF && --timeout) {}
    if(!timeout)spi_fault=1;
    return SSP1BUF;
}

/* Read-only flash access during initial board bring-up. */
uint32_t flash_id(void)
{
    uint32_t id;
    spi_fault=0;
    LATCbits.LATC1 = 0;
    spi(0x9f);
    id = (uint32_t)spi(0) << 16;
    id |= (uint32_t)spi(0) << 8;
    id |= spi(0);
    LATCbits.LATC1 = 1;
    return spi_fault?0:id;
}

uint8_t flash_read(uint32_t address, uint8_t *data, uint16_t count)
{
    if (address >= 0x80000ul || count > 0x80000ul - address) return 0;
    spi_fault=0;
    LATCbits.LATC1 = 0;
    spi(0x03); spi((uint8_t)(address >> 16)); spi((uint8_t)(address >> 8)); spi((uint8_t)address);
    while (count--) *data++ = spi(0);
    LATCbits.LATC1 = 1;
    return !spi_fault;
}

uint8_t flash_status(uint8_t *status)
{
    spi_fault=0;LATCbits.LATC1=0;spi(5);*status=spi(0);LATCbits.LATC1=1;return !spi_fault;
}
static uint8_t flash_write_enable(void)
{
    uint8_t status;
    if(!flash_status(&status)||(status&1u)||flash_id()!=0xbf258dul)return 0;
    if(status&0xbcu) {
        /* SST datasheet EWSR/WRSR sequence, also used by original 0x017306. */
        spi_fault=0;LATCbits.LATC1=0;spi(0x50);LATCbits.LATC1=1;
        LATCbits.LATC1=0;spi(1);spi(0);LATCbits.LATC1=1;
        if(spi_fault || !flash_status(&status)||(status&0xbcu))return 0;
    }
    spi_fault=0;LATCbits.LATC1=0;spi(6);LATCbits.LATC1=1;
    return !spi_fault && flash_status(&status) && (status&2u);
}
static void flash_address(uint32_t address)
{
    spi((uint8_t)(address>>16));spi((uint8_t)(address>>8));spi((uint8_t)address);
}
uint8_t flash_erase_config_sector(uint32_t address)
{
    if(address!=CONFIG_SLOT_A && address!=CONFIG_SLOT_B)return 0;
    if(!flash_write_enable())return 0;
    spi_fault=0;LATCbits.LATC1=0;spi(0x20);flash_address(address);LATCbits.LATC1=1;return !spi_fault;
}
uint8_t flash_program_config_byte(uint32_t address,uint8_t value)
{
    uint32_t base=address&0xfffff000ul;
    if((base!=CONFIG_SLOT_A && base!=CONFIG_SLOT_B)||address-base>=CONFIG_RECORD_SIZE)return 0;
    if(!flash_write_enable())return 0;
    spi_fault=0;LATCbits.LATC1=0;spi(2);flash_address(address);spi(value);LATCbits.LATC1=1;return !spi_fault;
}
uint8_t flash_erase_asset_sector(uint8_t sector)
{
    if(sector>=WEB_ASSET_SIZE/WEB_ASSET_SECTOR_SIZE || !flash_write_enable())return 0;
    spi_fault=0;LATCbits.LATC1=0;spi(0x20);flash_address(WEB_ASSET_BASE+(uint32_t)sector*WEB_ASSET_SECTOR_SIZE);LATCbits.LATC1=1;return !spi_fault;
}
uint8_t flash_program_asset_byte(uint16_t offset,uint8_t value)
{
    if(offset>=WEB_ASSET_SIZE || !flash_write_enable())return 0;
    spi_fault=0;LATCbits.LATC1=0;spi(2);flash_address(WEB_ASSET_BASE+offset);spi(value);LATCbits.LATC1=1;return !spi_fault;
}

#include "ota_store.h"
uint8_t ota_flash_read(uint32_t address,uint8_t *p,uint16_t n){return address<0x3c000ul && n<=0x3c000ul-address && flash_read(address,p,n);}
uint8_t ota_flash_ready(void){uint8_t s;return flash_status(&s)&&!(s&1u);}
uint8_t ota_flash_wait(void){uint32_t timeout=2000000ul;while(timeout--){if(ota_flash_ready())return 1;ClrWdt();}return 0;}
uint8_t ota_flash_program(uint32_t address,uint8_t value){if(address>=0x3c000ul||!flash_write_enable())return 0;spi_fault=0;LATCbits.LATC1=0;spi(2);flash_address(address);spi(value);LATCbits.LATC1=1;return !spi_fault;}
uint8_t ota_flash_erase(uint32_t address){if(address>=0x3c000ul||(address&4095ul)||!flash_write_enable())return 0;spi_fault=0;LATCbits.LATC1=0;spi(0x20);flash_address(address);LATCbits.LATC1=1;return !spi_fault;}
#ifdef OEM_RESTORE_BUILD
/* One-off converter only. Upper scratch receives the OEM PIC image; lower SPI
 * receives the stock loader's ASCII staging file. Config/web sectors excluded. */
static uint8_t restore_range(uint32_t a){return a<0x4f000ul||(a>=0x60000ul&&a<0x80000ul);}
uint8_t restore_flash_program(uint32_t a,uint8_t v){if(!restore_range(a)||!flash_write_enable())return 0;spi_fault=0;LATCbits.LATC1=0;spi(2);flash_address(a);spi(v);LATCbits.LATC1=1;return !spi_fault;}
uint8_t restore_flash_erase(uint32_t a){if(!restore_range(a)||(a&4095ul)||!flash_write_enable())return 0;spi_fault=0;LATCbits.LATC1=0;spi(0x20);flash_address(a);LATCbits.LATC1=1;return !spi_fault;}
#endif
