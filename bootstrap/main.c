/* One-time OEM-to-clean converter. Runs below 0x6000 while replacing the
 * upper loader. Stable external power and ICSP recovery are required. */
#include <xc.h>
#include <stdint.h>
#include "ota_store.h"
#include "transition_image.h"
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

/* All compiler constants are linked in bank zero; blob reads explicitly span
 * both banks and restore the compiler's table-pointer invariant. */
static void pic_read(uint32_t a,uint8_t *p,uint8_t n){
 TBLPTRU=(uint8_t)(a>>16);TBLPTRH=(uint8_t)(a>>8);TBLPTRL=(uint8_t)a;
 while(n--){asm("tblrd*+");*p++=TABLAT;}TBLPTRU=0;
}
static uint8_t row[64],check[64],journal[32];
static uint32_t source,source_end;static uint8_t remaining,ff_run;
static uint8_t raw(void){uint8_t b;if(source>=source_end)stop();pic_read(source++,&b,1);return b;}
static void begin(uint32_t start,uint32_t n){source=start;source_end=start+n;remaining=0;}
static uint8_t next(void){uint8_t token;if(!remaining){token=raw();remaining=(token&127u)+1u;ff_run=token&128u;}--remaining;return ff_run?255:raw();}
static void end(void){if(remaining||source!=source_end)stop();}
static void program(uint32_t a,uint8_t v){if(v!=255&&(!ota_flash_program(a,v)||!ota_flash_wait()))stop();ClrWdt();}
static void erase(uint32_t a){if(!ota_flash_erase(a)||!ota_flash_wait())stop();ClrWdt();}
static void unlock(void){EECON2=0x55;EECON2=0xaa;EECON1bits.WR=1;asm("nop");asm("nop");EECON1bits.WREN=0;}
static void loader_row(uint32_t a){uint8_t i;
 if(a<OTA_BOOT_BASE||a>=0x20000ul||(a&63ul))stop();
 TBLPTRU=(uint8_t)(a>>16);TBLPTRH=(uint8_t)(a>>8);TBLPTRL=(uint8_t)a;
 if(!(a&1023ul)){EECON1bits.WREN=1;EECON1bits.FREE=1;unlock();}
 for(i=0;i<64;++i){TABLAT=row[i];asm("tblwt*+");}
 asm("tblrd*-");EECON1bits.FREE=0;EECON1bits.WREN=1;unlock();TBLPTRU=0;ClrWdt();
 pic_read(a,check,64);
 for(i=0;i<64;++i){uint8_t mask=255;
  /* Unimplemented configuration bits need not read back as written. */
  if(a+i>=0x1fff8ul){switch(i){case 56:mask=0xe1;break;case 57:mask=4;break;case 58:mask=0xc7;break;case 59:mask=15;break;case 60:mask=0;break;case 61:mask=4;break;default:mask=0;break;}}
  if((row[i]&mask)!=(check[i]&mask))stop();
 }
}
void main(void){uint32_t a,c,want;uint8_t i;
 INTCON=0;WDTCONbits.SWDTEN=0;ADCON1=15;CMCON=7;OSCTUNE=0x40;TBLPTRU=0;
 LATFbits.LATF3=1;LATDbits.LATD1=0;TRISFbits.TRISF3=0;TRISDbits.TRISD1=0;
 LATCbits.LATC1=1;TRISCbits.TRISC1=0;LATCbits.LATC0=1;TRISCbits.TRISC0=0;
 TRISCbits.TRISC3=0;TRISCbits.TRISC4=1;TRISCbits.TRISC5=0;SSP1STAT=0x40;SSP1CON1=0x21;
 LATCbits.LATC1=0;spi(0x9f);if(spi(255)!=0xbf||spi(255)!=0x25||spi(255)!=0x8d)stop();LATCbits.LATC1=1;
 /* Validate the complete embedded loader before touching either loader page. */
 begin(TRANSITION_BLOB_BASE+TRANSITION_APP_PACKED,TRANSITION_LOADER_PACKED);c=0xfffffffful;
 for(a=OTA_BOOT_BASE;a<0x20000ul;a+=64){for(i=0;i<64;++i)row[i]=next();c=ota_crc(c,row,64);ClrWdt();}
 end();if(~c!=TRANSITION_LOADER_FULL_CRC)stop();
 /* Discard old journals and alternate slot before publishing the new active
  * slot: stale candidates must never be eligible for short-boot recovery. */
 erase(OTA_JOURNAL_A);erase(OTA_JOURNAL_B);
 for(a=OTA_SLOT_SIZE;a<2ul*OTA_SLOT_SIZE;a+=4096)erase(a);
 for(a=0;a<OTA_SLOT_SIZE;a+=4096)erase(a);
 begin(TRANSITION_BLOB_BASE,TRANSITION_APP_PACKED);
 for(a=0;a<OTA_APP_SIZE+64ul;++a){uint8_t b=next();if(a!=63)program(a,b);}
 end();if(!ota_flash_read(0,row,64))stop();want=ota_u32(row+20);c=0xfffffffful;
 for(a=0;a<OTA_APP_SIZE;a+=64){if(!ota_flash_read(64+a,row,64))stop();c=ota_crc(c,row,64);ClrWdt();}
 if(~c!=want)stop();program(63,0xa5);
 if(!ota_flash_read(0,row,64)||!ota_header_valid(row))stop();
 ota_record(journal,1,0,OTA_NONE,OTA_CONFIRMED);
 for(i=0;i<32;++i)program(OTA_JOURNAL_A+i,journal[i]);
 if(!ota_flash_read(OTA_JOURNAL_A,row,32)||!ota_record_valid(row))stop();
 /* From this point a power cut may require ICSP. Never erase below BOOT_BASE:
  * that protects the executing converter and its compressed input. */
 LATDbits.LATD1=1;
 begin(TRANSITION_BLOB_BASE+TRANSITION_APP_PACKED,TRANSITION_LOADER_PACKED);
 for(a=OTA_BOOT_BASE;a<0x20000ul;a+=64){for(i=0;i<64;++i)row[i]=next();loader_row(a);}
 end();c=0xfffffffful;
 for(a=OTA_BOOT_BASE;a<0x1ffc0ul;a+=64){pic_read(a,row,64);c=ota_crc(c,row,64);ClrWdt();}
 pic_read(0x1ffc0ul,row,56);c=ota_crc(c,row,56);if(~c!=TRANSITION_LOADER_CRC)stop();
 Reset();for(;;)ClrWdt();
}
