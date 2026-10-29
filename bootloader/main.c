#include <xc.h>
#include <stdint.h>
#include <string.h>
#include "ota_store.h"
#pragma config WDT=OFF, STVR=ON, XINST=OFF, DEBUG=OFF, CP0=OFF
#pragma config FOSC=HSPLL, FOSC2=ON, FCMEN=OFF, IESO=OFF, ETHLED=ON
#pragma config WDTPS=8192
#include "boot_platform.h"
static void stop(void){LATFbits.LATF3=1;LATDbits.LATD1=0;for(;;)ClrWdt();}
static uint8_t spi(uint8_t v){uint16_t t=65535;PIR1bits.SSP1IF=0;SSP1BUF=v;while(!PIR1bits.SSP1IF&&--t){}if(!t)stop();return SSP1BUF;}
static void address(uint32_t a){spi((uint8_t)(a>>16));spi((uint8_t)(a>>8));spi((uint8_t)a);}
uint8_t ota_flash_read(uint32_t a,uint8_t *p,uint16_t n){if(a>=0x3c000ul||n>0x3c000ul-a)return 0;LATCbits.LATC1=0;spi(3);address(a);while(n--)*p++=spi(255);LATCbits.LATC1=1;return 1;}
uint8_t ota_flash_ready(void){uint8_t s;LATCbits.LATC1=0;spi(5);s=spi(255);LATCbits.LATC1=1;return !(s&1u);}
uint8_t ota_flash_wait(void){uint32_t n=2000000ul;while(n--){if(ota_flash_ready())return 1;ClrWdt();}return 0;}
static uint8_t enable(void){uint8_t s;if(!ota_flash_wait())return 0;LATCbits.LATC1=0;spi(5);s=spi(255);LATCbits.LATC1=1;if(s&0xbc){LATCbits.LATC1=0;spi(0x50);LATCbits.LATC1=1;LATCbits.LATC1=0;spi(1);spi(0);LATCbits.LATC1=1;if(!ota_flash_wait())return 0;}
 LATCbits.LATC1=0;spi(6);LATCbits.LATC1=1;LATCbits.LATC1=0;spi(5);s=spi(255);LATCbits.LATC1=1;return (s&0xbe)==2;}
uint8_t ota_flash_program(uint32_t a,uint8_t v){if(a>=0x3c000ul||!enable())return 0;LATCbits.LATC1=0;spi(2);address(a);spi(v);LATCbits.LATC1=1;return 1;}
uint8_t ota_flash_erase(uint32_t a){if(a>=0x3c000ul||(a&4095ul)||!enable())return 0;LATCbits.LATC1=0;spi(0x20);address(a);LATCbits.LATC1=1;return 1;}
/* XC8's small-constant helpers load TBLPTRH/L but assume TBLPTRU is already
 * the linked constant bank. Reset leaves U=0; our loader constants are in bank
 * 1. Raw application table reads/writes must restore this ABI invariant. */
/* The loader is linked entirely in program bank 1. Keep this a C SFR write
 * so XC8 also accounts for the WREG clobber in its call graph. */
static void table_bank(void){TBLPTRU=1;}
void boot_pic_read(uint32_t a,uint8_t *p,uint8_t n){TBLPTRU=(uint8_t)(a>>16);TBLPTRH=(uint8_t)(a>>8);TBLPTRL=(uint8_t)a;while(n--){asm("tblrd*+");*p++=TABLAT;}table_bank();}
static void unlock(void){EECON2=0x55;EECON2=0xaa;EECON1bits.WR=1;asm("nop");asm("nop");EECON1bits.WREN=0;}
uint8_t boot_pic_row(uint32_t a,const uint8_t *p){uint8_t i;
 if(a<OTA_APP_BASE||a>=OTA_BOOT_BASE||(a&63ul))return 0;
 TBLPTRU=(uint8_t)(a>>16);TBLPTRH=(uint8_t)(a>>8);TBLPTRL=(uint8_t)a;
 if(!(a&1023ul)){EECON1bits.WREN=1;EECON1bits.FREE=1;unlock();}
 for(i=0;i<64;++i){TABLAT=p[i];asm("tblwt*+");}
 asm("tblrd*-");EECON1bits.FREE=0;EECON1bits.WREN=1;unlock();ClrWdt();table_bank();return 1;
}
void boot_reboot(void){Reset();}
#ifdef BOOT_BENCH_TRACE
void boot_trace(uint8_t stage,uint8_t value){ota_flash_program(OTA_JOURNAL_B+stage,value);ota_flash_wait();}
#endif
void main(void){
 INTCON=0;WDTCONbits.SWDTEN=0;ADCON1=15;CMCON=7;OSCTUNE=0x40;
 LATFbits.LATF3=1;LATDbits.LATD1=0;TRISFbits.TRISF3=0;TRISDbits.TRISD1=0;
 LATCbits.LATC1=1;TRISCbits.TRISC1=0;LATCbits.LATC0=1;TRISCbits.TRISC0=0;
 TRISCbits.TRISC3=0;TRISCbits.TRISC4=1;TRISCbits.TRISC5=0;SSP1STAT=0x40;SSP1CON1=0x21;
 LATCbits.LATC1=0;spi(0x9f);if(spi(255)!=0xbf||spi(255)!=0x25||spi(255)!=0x8d)stop();LATCbits.LATC1=1;

#ifdef BOOT_BENCH_TRACE
 if(!ota_flash_erase(OTA_JOURNAL_B)||!ota_flash_wait())stop();boot_trace(0,0xb0);boot_trace(10,RCON);
#endif
 table_bank();if(!boot_run())stop();
 LATFbits.LATF3=0;SSP1CON1=0;INTCON=0;STKPTR=0;asm("goto 0x400");
}
