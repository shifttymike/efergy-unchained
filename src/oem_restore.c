#ifdef OEM_RESTORE_BUILD
#include "TCPIP Stack/TCPIP.h"
#include "board.h"
#include "config_store.h"
#include "web_assets.h"
#include "ota_update.h"
#include "ota_store.h"
#include "oem_restore.h"
#include "oem_hex.h"
#include "oem_image.h"
#define RAW_BASE 0x60000ul
#define RAW_SIZE 0x20000ul
static uint8_t state,error_code,buffer[64];
static uint16_t count,byte_index;
static const uint8_t *chunk;
static uint32_t position,crc,verify_crc;
#ifndef OEM_RESTORE_HOST_TEST
static uint8_t check[64];
static uint32_t hex_position,hex_crc;
#endif
static DWORD started;
static void fail(uint8_t e){error_code=e;state=255;chunk=0;}
uint8_t oem_restore_busy(void){return state&&state!=255;}
uint8_t oem_restore_chunk_pending(void){return state==4||state==5;}
uint8_t oem_restore_failed(void){return state==255;}
void oem_restore_abort(void){if(state==4||state==5){chunk=0;fail(8);}}
uint16_t oem_restore_status(char *p,uint16_t cap){return (uint16_t)snprintf(p,cap,"{\"state\":%u,\"error\":%u,\"offset\":%lu,\"size\":131072,\"ready\":%s}",state,error_code,(unsigned long)position,state==6?"true":"false");}
uint16_t oem_restore_begin(const uint8_t *p,uint16_t n){uint8_t r[32];uint32_t a;
 if(config_store_busy()||web_assets_busy()||ota_update_busy()||(state&&state!=3&&state!=255))return 409;
 if(!ota_find_record(r,&a)||r[10]!=OTA_CONFIRMED||r[11])return 409;
 if(n!=64||memcmp(p,oem_restore_header,64))return 400;
 state=1;error_code=0;position=0;crc=0xfffffffful;return 202;
}
uint16_t oem_restore_chunk(const uint8_t *p,uint16_t n){if(state!=3)return 409;
 if(n<5||n>260||ota_u32(p)!=position||position+(uint32_t)(n-4)>RAW_SIZE)return 400;
 chunk=p+4;count=n-4;byte_index=0;state=4;return 202;
}
uint16_t oem_restore_commit(void){if(state!=6||config_store_busy()||web_assets_busy()||ota_update_busy())return 409;state=7;started=TickGet();return 202;}
#ifndef OEM_RESTORE_HOST_TEST
static void stop(void){LATFbits.LATF3=1;LATDbits.LATD1=0;for(;;)ClrWdt();}
static void write_byte(uint32_t a,uint8_t b){if(!restore_flash_program(a,b)||!ota_flash_wait())stop();ClrWdt();}
static uint8_t source_read(uint32_t a,uint8_t *p,uint16_t n){return a<RAW_SIZE&&n<=RAW_SIZE-a&&flash_read(RAW_BASE+a,p,n);}
static uint8_t emit(uint8_t b){if(hex_position>=OEM_HEX_LENGTH)return 0;write_byte(0x1000ul+hex_position++,b);hex_crc=ota_crc(hex_crc,&b,1);return 1;}
static void unlock(void){EECON2=0x55;EECON2=0xaa;EECON1bits.WR=1;asm("nop");asm("nop");EECON1bits.WREN=0;}
/* Preserve the compiler's table-pointer bank across explicit PIC accesses.
 * The large converter and the small permanent loader use different const ABIs. */
static void loader_row(uint32_t a){uint8_t i,b,mask,saved_bank=TBLPTRU;
 if(a<0x1cc00ul||a>=0x20000ul||(a&63ul))stop();
 if(!source_read(a,buffer,64))stop();
 TBLPTRU=(uint8_t)(a>>16);TBLPTRH=(uint8_t)(a>>8);TBLPTRL=(uint8_t)a;
 if(!(a&1023ul)){EECON1bits.WREN=1;EECON1bits.FREE=1;unlock();}
 for(i=0;i<64;++i){TABLAT=buffer[i];asm("tblwt*+");}
 asm("tblrd*-");EECON1bits.FREE=0;EECON1bits.WREN=1;unlock();ClrWdt();
 TBLPTRU=(uint8_t)(a>>16);TBLPTRH=(uint8_t)(a>>8);TBLPTRL=(uint8_t)a;
 for(i=0;i<64;++i){asm("tblrd*+");check[i]=TABLAT;}
 TBLPTRU=saved_bank;
 for(i=0;i<64;++i){b=check[i];mask=255;
  if(a+i>=0x1fff8ul){switch(i){case 56:mask=0xe1;break;case 57:mask=4;break;case 58:mask=0xc7;break;case 59:mask=15;break;case 60:mask=0;break;case 61:mask=4;break;default:mask=0;break;}}
  if((b&mask)!=(buffer[i]&mask))stop();
 }
}
static void install(void){uint32_t a,c;uint8_t i;char marker[64];
 /* Raw upload was verified before this point. From here the custom recovery
  * slots/journal are replaced; interruption may require ICSP recovery. */
 INTCON=0;LATFbits.LATF3=1;LATDbits.LATD1=0;
 for(a=0;a<0x1000ul+OEM_HEX_LENGTH;a+=4096)if(!restore_flash_erase(a)||!ota_flash_wait())stop();
 hex_position=0;hex_crc=0xfffffffful;
 if(!oem_hex_generate(source_read,emit)||hex_position!=OEM_HEX_LENGTH||~hex_crc!=OEM_HEX_CRC)stop();
 c=0xfffffffful;
 for(a=0;a<OEM_HEX_LENGTH;){uint16_t n=(uint16_t)((OEM_HEX_LENGTH-a)>64?64:OEM_HEX_LENGTH-a);if(!flash_read(0x1000ul+a,buffer,n))stop();c=ota_crc(c,buffer,n);a+=n;ClrWdt();}
 if(~c!=OEM_HEX_CRC)stop();
 snprintf(marker,sizeof(marker),"HEXFILE,%08lu,%s",(unsigned long)OEM_HEX_LENGTH,OEM_HEX_MD5);
 for(i=1;marker[i];++i)write_byte(i,(uint8_t)marker[i]);write_byte(0,(uint8_t)marker[0]);
 for(a=0x1cc00ul;a<0x20000ul;a+=64)loader_row(a);
 Reset();for(;;)ClrWdt();
}
#else
extern void oem_restore_test_install(void);
#define install oem_restore_test_install
#endif
void oem_restore_task(void){uint8_t b;DWORD now=TickGet();
 if(state==1){if(!restore_flash_erase(RAW_BASE+position)){fail(1);return;}started=now;state=2;return;}
 if(state==2||state==5){
  if(!ota_flash_ready()){if((DWORD)(now-started)>TICK_SECOND)fail(2);return;}
  if(state==2){position+=4096;if(position<RAW_SIZE)state=1;else{position=0;state=3;}return;}
  if(!flash_read(RAW_BASE+position+byte_index,&b,1)||b!=chunk[byte_index]){fail(3);return;}
  if(++byte_index==count){crc=ota_crc(crc,chunk,count);position+=count;chunk=0;state=position==RAW_SIZE?8:3;if(state==8){position=0;verify_crc=0xfffffffful;}}
  else state=4;return;
 }
 if(state==4){if(!restore_flash_program(RAW_BASE+position+byte_index,chunk[byte_index])){fail(1);return;}started=now;state=5;return;}
 if(state==8){if(!flash_read(RAW_BASE+position,buffer,64)){fail(3);return;}verify_crc=ota_crc(verify_crc,buffer,64);position+=64;
  if(position==RAW_SIZE){if(~verify_crc!=OEM_RAW_CRC||~crc!=OEM_RAW_CRC){fail(4);return;}state=6;}return;
 }
 if(state==7&&(DWORD)(now-started)>2ul*TICK_SECOND)install();
}
#endif
