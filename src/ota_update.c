#include "TCPIP Stack/TCPIP.h"
#include "app.h"
#include "ota_update.h"
#include "ota_store.h"
#include "config_store.h"
#include "web_assets.h"
/* Bench-only fault injection; never enable in a release build. */
#ifndef OTA_BENCH_NO_CONFIRM
#define OTA_BENCH_NO_CONFIRM 0
#endif
/* HTTP keeps its body buffer owned until chunk completion; no second packet buffer. */
static uint8_t header[64],record[32];
static const uint8_t *chunk;
static uint32_t journal,position,operation,crc,expected,started,trial_started,verify_crc;
static uint16_t byte_index,count;
static uint8_t state,error_code,target,erase_needed,confirming,rollback_ready,boot_trial;
/* 0 idle, 1 erase, 2 erase wait, 3 upload, 4 byte write, 5 byte verify,
 * 6 image verify, 7 header, 8 header wait, 9 journal prepare,
 * 10 journal erase wait, 11 journal, 12 journal wait, 13 committed, 255 error. */
static void failed(uint8_t e){error_code=e;state=255;chunk=0;}
uint8_t ota_update_busy(void){return state && state!=255 && state!=13;}
uint8_t ota_update_chunk_pending(void){return state==4||state==5;}
uint8_t ota_update_failed(void){return state==255;}
void ota_update_init(void){state=0;error_code=0;trial_started=TickGet();if(!ota_find_record(record,&journal)){memset(record,255,32);journal=0;}boot_trial=ota_record_valid(record)&&record[10]==OTA_TRIAL;}
void ota_update_abort(void){if(state!=13&&record[10]==OTA_CONFIRMED){state=0;chunk=0;error_code=0;}}
static uint8_t stable(void){return !config_store_busy()&&!web_assets_busy();}
uint16_t ota_update_begin(const uint8_t *p,uint16_t n){
 if(!stable()||(ota_update_busy()&&state!=3)||state==13)return 409;
 if(!ota_record_valid(record)||record[10]!=OTA_CONFIRMED)return 503;
 if(n!=64||!ota_header_valid(p))return 400;
 memcpy(header,p,64);expected=ota_u32(header+20);target=record[8]^1u;
 state=1;position=0;error_code=0;confirming=0;rollback_ready=0;return 202;
}
uint16_t ota_update_chunk(const uint8_t *p,uint16_t n){
 if(state!=3)return 409;
 if(n<5||n>260||ota_u32(p)!=position||position+(uint32_t)(n-4)>OTA_APP_SIZE)return 400;
 chunk=p+4;count=n-4;byte_index=0;state=4;return 202;
}
uint16_t ota_update_commit(void){if(state!=3||position!=OTA_APP_SIZE)return 409;position=0;verify_crc=0xfffffffful;state=6;return 202;}
uint16_t ota_update_rollback(void){if(!stable()||ota_update_busy()||!ota_record_valid(record)||record[10]!=OTA_CONFIRMED)return 409;
 target=record[8]^1u;position=0;verify_crc=0xfffffffful;rollback_ready=1;confirming=0;
 if(!ota_flash_read((uint32_t)target*OTA_SLOT_SIZE,header,64)||!ota_header_valid(header)){rollback_ready=0;return 404;}expected=ota_u32(header+20);state=6;return 202;}
uint16_t ota_update_status(char *out,uint16_t cap){return (uint16_t)snprintf(out,cap,"{\"state\":%u,\"error\":%u,\"offset\":%lu,\"size\":%lu,\"active_slot\":%u,\"phase\":%u,\"journal_address\":%lu,\"operation_address\":%lu,\"journal_sequence\":%lu,\"ready_to_reboot\":%s}",state,error_code,(unsigned long)position,(unsigned long)OTA_APP_SIZE,record[8],record[10],(unsigned long)journal,(unsigned long)operation,(unsigned long)ota_u32(record+4),state==13?"true":"false");}
void ota_update_task(void){uint8_t b,p[32],i;uint32_t a;DWORD now=TickGet();
 /* Confirmation requires a responsive main loop and an established network. An
  * unconfirmed trial resets after 120s even if the main loop is still running. */
 if(boot_trial&&(DWORD)(now-trial_started)>120ul*TICK_SECOND){app_reboot_requested=1;return;}
 if(ota_record_valid(record)&&state==0&&(record[10]==OTA_TRIAL||record[11])){
  if((DWORD)(now-trial_started)>120ul*TICK_SECOND){app_reboot_requested=1;return;}
  if((DWORD)(now-trial_started)>45ul*TICK_SECOND&&(!boot_trial||(app_network_ready&&!OTA_BENCH_NO_CONFIRM))&&web_assets_ready()&&stable()){
   ota_record(record,ota_u32(record+4)+1,boot_trial?record[9]:record[8],OTA_NONE,OTA_CONFIRMED);confirming=1;state=9;
  }
 }
 if(!state||state==3||state==13||state==255)return;
 if(!stable())return;
 if(state==1){a=(uint32_t)target*OTA_SLOT_SIZE+position;if(!ota_flash_erase(a)){failed(1);return;}started=now;state=2;return;}
 if(state==2||state==5||state==8||state==10||state==12){
  if(!ota_flash_ready()){if((DWORD)(now-started)>TICK_SECOND)failed(2);return;}
  if(state==2){position+=4096;if(position<OTA_SLOT_SIZE)state=1;else{position=0;crc=0xfffffffful;state=3;}return;}
  if(state==5){a=(uint32_t)target*OTA_SLOT_SIZE+64+position+byte_index;if(!ota_flash_read(a,&b,1)||b!=chunk[byte_index]){failed(3);return;}if(++byte_index==count){crc=ota_crc(crc,chunk,count);position+=count;chunk=0;state=3;}else state=4;return;}
  if(state==8){if(!ota_flash_read((uint32_t)target*OTA_SLOT_SIZE+byte_index,&b,1)||b!=header[byte_index]){failed(3);return;}if(++byte_index==64)state=9;else state=7;return;}
  if(state==10){state=11;byte_index=0;return;}
  if(state==12){if(!ota_flash_read(operation+byte_index,&b,1)||b!=record[byte_index]){failed(3);return;}if(++byte_index==32){journal=operation;state=confirming?0:13;if(confirming)boot_trial=0;confirming=0;}else state=11;return;}
 }
 if(state==4){a=(uint32_t)target*OTA_SLOT_SIZE+64+position+byte_index;if(!ota_flash_program(a,chunk[byte_index])){failed(1);return;}started=now;state=5;return;}
 if(state==6){
  if(!ota_flash_read((uint32_t)target*OTA_SLOT_SIZE+64+position,p,32)){failed(3);return;}verify_crc=ota_crc(verify_crc,p,32);position+=32;
  if(position<OTA_APP_SIZE)return;
  if(~verify_crc!=expected||(!rollback_ready&&~crc!=expected)){failed(4);return;}
  byte_index=0;state=rollback_ready?9:7;return;
 }
 if(state==7){if(!ota_flash_program((uint32_t)target*OTA_SLOT_SIZE+byte_index,header[byte_index])){failed(1);return;}started=now;state=8;return;}
 if(state==9){
  if(!confirming)ota_record(record,ota_u32(record+4)+1,record[8],target,OTA_PENDING);
  if(!ota_next_record(journal,&operation,&erase_needed)){failed(5);return;}
  if(erase_needed){if(!ota_flash_erase(operation)){failed(1);return;}started=now;state=10;}else{byte_index=0;state=11;}return;
 }
 if(state==11){i=record[byte_index];if(!ota_flash_program(operation+byte_index,i)){failed(1);return;}started=now;state=12;return;}
}
