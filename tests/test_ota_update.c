#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "TCPIP Stack/TCPIP.h"
#include "ota_store.h"
#include "ota_update.h"
static uint8_t flash[0x80000],package[OTA_APP_SIZE+64],body[260],rec[32];
static DWORD now;
uint8_t app_network_ready=1,app_reboot_requested;
static uint8_t blocked,stuck;
DWORD TickGet(void){return now;}
uint8_t config_store_busy(void){return blocked;}
uint8_t web_assets_busy(void){return 0;}
uint8_t web_assets_ready(void){return 1;}
uint8_t ota_flash_read(uint32_t a,uint8_t *p,uint16_t n){assert(a+n<=0x3c000);memcpy(p,flash+a,n);return 1;}
uint8_t ota_flash_program(uint32_t a,uint8_t v){assert(a<0x3c000);assert((flash[a]&v)==v);flash[a]&=v;return 1;}
uint8_t ota_flash_erase(uint32_t a){assert(a<0x3c000&&!(a&4095));memset(flash+a,255,4096);return 1;}
uint8_t ota_flash_wait(void){return 1;}
uint8_t ota_flash_ready(void){return !stuck;}
static void task(void){++now;ota_update_task();}
static void run(void){unsigned guard=100000;while(ota_update_busy()&&guard--){char out[256];ota_update_status(out,sizeof(out));if(strstr(out,"\"state\":3,"))return;task();}assert(guard);}
static void init(void){memset(flash,255,sizeof(flash));memcpy(flash,package,sizeof(package));ota_record(rec,1,0,OTA_NONE,OTA_CONFIRMED);memcpy(flash+OTA_JOURNAL_A,rec,32);now=0;stuck=blocked=app_reboot_requested=0;app_network_ready=1;ota_update_init();}
static void upload(uint8_t corrupt){assert(ota_update_begin(package,64)==202);run();for(uint32_t a=0;a<OTA_APP_SIZE;a+=256){ota_put32(body,a);memcpy(body+4,package+64+a,256);if(corrupt&&a==0)body[100]^=1;assert(ota_update_chunk(body,260)==202);while(ota_update_chunk_pending())task();assert(!ota_update_failed());}}
int main(void){FILE *f=fopen("build/efergy-update.efu","rb");assert(f);assert(fread(package,1,sizeof(package),f)==sizeof(package));fclose(f);init();
 blocked=1;assert(ota_update_begin(package,64)==409);blocked=0;
 package[8]^=1;assert(ota_update_begin(package,64)==400);package[8]^=1;
 assert(ota_update_begin(package,64)==202);run();ota_put32(body,256);assert(ota_update_chunk(body,260)==400);assert(ota_update_commit()==409);
 ota_update_abort();assert(!ota_update_busy());
 assert(ota_update_begin(package,64)==202);run();ota_put32(body,0);memcpy(body+4,package+64,256);assert(ota_update_chunk(body,260)==202);task();
 ota_update_abort();memset(body,0,sizeof(body));for(unsigned j=0;j<10;j++)task();assert(!ota_update_chunk_pending()&&!ota_update_busy());
 upload(0);assert(ota_update_commit()==202);run();assert(!ota_update_failed());uint32_t address;assert(ota_find_record(rec,&address)&&rec[10]==OTA_PENDING&&rec[9]==1);assert(!memcmp(flash+OTA_SLOT_SIZE,package,sizeof(package)));
 /* A staged commit cannot be silently cancelled or overwritten. */
 ota_update_abort();assert(ota_update_begin(package,64)==409);
 init();upload(1);assert(ota_update_commit()==202);run();assert(ota_update_failed());assert(ota_find_record(rec,&address)&&rec[10]==OTA_CONFIRMED);assert(flash[OTA_SLOT_SIZE+63]==255);
 init();assert(ota_update_begin(package,64)==202);task();stuck=1;now+=2*TICK_SECOND;task();assert(ota_update_failed());
 /* A failed trial gets a bounded main-loop deadline, not just a watchdog. */
 init();memcpy(flash+OTA_SLOT_SIZE,package,sizeof(package));ota_record(rec,2,0,1,OTA_TRIAL);memcpy(flash+OTA_JOURNAL_A+32,rec,32);ota_update_init();app_network_ready=0;now=121ul*TICK_SECOND;task();assert(app_reboot_requested);
 init();memcpy(flash+OTA_SLOT_SIZE,package,sizeof(package));ota_record(rec,2,0,1,OTA_TRIAL);memcpy(flash+OTA_JOURNAL_A+32,rec,32);ota_update_init();now=46ul*TICK_SECOND;task();run();assert(ota_find_record(rec,&address)&&rec[10]==OTA_CONFIRMED&&rec[8]==1);
 for(unsigned i=0x3c000;i<sizeof(flash);i++)assert(flash[i]==255);
 puts("OTA updater: streamed full image/readback, bounds/order checks, locks, abort, corrupt CRC refusal, flash timeout, durable commit and trial confirmation/deadline passed");
}
