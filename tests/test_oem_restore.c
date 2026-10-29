#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "TCPIP Stack/TCPIP.h"
#include "oem_restore.h"
#include "ota_format.h"
#include "oem_image.h"
static uint8_t flash[0x80000],raw[0x20000],body[260],blocked,stuck,trial,installed;
static DWORD now;
DWORD TickGet(void){return now;}
uint8_t config_store_busy(void){return blocked;}
uint8_t web_assets_busy(void){return 0;}
uint8_t ota_update_busy(void){return 0;}
uint8_t ota_find_record(uint8_t *p,uint32_t *a){ota_record(p,1,0,255,trial?2:0);*a=0x3a000;return 1;}
uint8_t ota_flash_ready(void){return !stuck;}
uint8_t flash_read(uint32_t a,uint8_t *p,uint16_t n){assert(a+n<=sizeof(flash));memcpy(p,flash+a,n);return 1;}
uint8_t restore_flash_program(uint32_t a,uint8_t v){assert(a>=0x60000&&a<0x80000);assert((flash[a]&v)==v);flash[a]&=v;return 1;}
uint8_t restore_flash_erase(uint32_t a){assert(a>=0x60000&&a<0x80000&&!(a&4095));memset(flash+a,255,4096);return 1;}
void oem_restore_test_install(void){++installed;}
static void task(void){++now;oem_restore_task();}
static uint8_t state(void){char out[180];oem_restore_status(out,sizeof(out));unsigned n;assert(sscanf(out,"{\"state\":%u",&n)==1);return (uint8_t)n;}
static void ready(void){unsigned guard=10000;while(state()!=3&&!oem_restore_failed()&&guard--)task();assert(guard);}
static void upload(uint8_t corrupt){assert(oem_restore_begin(oem_restore_header,64)==202);ready();for(uint32_t a=0;a<sizeof(raw);a+=256){ota_put32(body,a);memcpy(body+4,raw+a,256);if(corrupt&&!a)body[50]^=1;assert(oem_restore_chunk(body,260)==202);while(oem_restore_chunk_pending())task();}while(state()==8)task();}
int main(void){FILE *f=fopen("build/oem-restore/oem-pic.bin","rb");assert(f&&fread(raw,1,sizeof(raw),f)==sizeof(raw));fclose(f);memset(flash,0x5a,sizeof(flash));
 blocked=1;assert(oem_restore_begin(oem_restore_header,64)==409);blocked=0;trial=1;assert(oem_restore_begin(oem_restore_header,64)==409);trial=0;
 uint8_t bad[64];memcpy(bad,oem_restore_header,64);bad[12]^=1;assert(oem_restore_begin(bad,64)==400);
 assert(oem_restore_begin(oem_restore_header,64)==202);ready();ota_put32(body,256);assert(oem_restore_chunk(body,260)==400);assert(oem_restore_commit()==409);
 ota_put32(body,0);memcpy(body+4,raw,256);assert(oem_restore_chunk(body,260)==202);task();oem_restore_abort();memset(body,0,sizeof(body));task();assert(oem_restore_failed());
 assert(oem_restore_begin(oem_restore_header,64)==202);task();stuck=1;now+=2001;task();assert(oem_restore_failed());stuck=0;
 upload(1);assert(oem_restore_failed());assert(oem_restore_commit()==409);
 upload(0);assert(state()==6&&!oem_restore_failed());assert(!memcmp(flash+0x60000,raw,sizeof(raw)));
 for(unsigned a=0;a<0x60000;++a)assert(flash[a]==0x5a);
 assert(oem_restore_commit()==202);now+=1000;task();assert(!installed);now+=2001;task();assert(installed==1);
 puts("OEM restore: exact full-image upload/readback, CRC refusal, interruption, order/bounds, trial/config locks, untouched active slots/config and delayed commit passed");
}
