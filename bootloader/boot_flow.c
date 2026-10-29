#include <stdint.h>
#include <string.h>
#include "ota_store.h"
#include "boot_platform.h"
#include "factory_image.h"
static uint8_t scratch[64],record[32];
static uint32_t pic_crc(void){uint32_t a,c=0xfffffffful;for(a=OTA_APP_BASE;a<OTA_BOOT_BASE;a+=64){boot_pic_read(a,scratch,64);c=ota_crc(c,scratch,64);}return ~c;}
static uint8_t install(uint8_t slot){uint32_t a,base=(uint32_t)slot*OTA_SLOT_SIZE,want;uint8_t i;
 if(!ota_slot_valid(slot,scratch))return 0;
 for(a=OTA_APP_BASE;a<OTA_BOOT_BASE;a+=64){
  if(!ota_flash_read(base+64+a-OTA_APP_BASE,scratch,64)||!boot_pic_row(a,scratch))return 0;
  boot_pic_read(a,scratch,64);
  for(i=0;i<64;++i){uint8_t b;if(!ota_flash_read(base+64+a-OTA_APP_BASE+i,&b,1)||b!=scratch[i])return 0;}
 }
 if(!ota_flash_read(base,scratch,64))return 0;want=ota_u32(scratch+20);return pic_crc()==want;
}
static uint8_t seed(void){uint32_t a;uint8_t i,b;
 /* Never turn an unknown/partially flashed application into the recovery image. */
 if(pic_crc()!=FACTORY_APP_CRC)return 0;
 for(a=0;a<OTA_SLOT_SIZE;a+=4096)if(!ota_flash_erase(a)||!ota_flash_wait())return 0;
 for(a=0;a<OTA_APP_SIZE;a+=64){boot_pic_read(OTA_APP_BASE+a,scratch,64);for(i=0;i<64;++i){if(!ota_flash_program(64+a+i,scratch[i])||!ota_flash_wait())return 0;}}
 memcpy(scratch,factory_header,64);
 for(i=0;i<64;++i)if(!ota_flash_program(i,scratch[i])||!ota_flash_wait()||!ota_flash_read(i,&b,1)||b!=scratch[i])return 0;
 if(!ota_slot_valid(0,scratch))return 0;
 if(!ota_flash_erase(OTA_JOURNAL_A)||!ota_flash_wait())return 0;
 ota_record(record,1,0,OTA_NONE,OTA_CONFIRMED);
 /* Append to an erased journal starting at offset zero. */
 for(i=0;i<32;++i)if(!ota_flash_program(OTA_JOURNAL_A+i,record[i])||!ota_flash_wait())return 0;
 return 1;
}

uint8_t boot_run(void){uint32_t last;uint8_t phase,active,candidate;
 uint8_t found=ota_find_record(record,&last);BOOT_TRACE(1,found);BOOT_TRACE(2,record[4]);BOOT_TRACE(3,record[10]);
 if(!found){BOOT_TRACE(8,0x55);if(!seed()||!ota_find_record(record,&last))return 0;}
 phase=record[10];active=record[8];candidate=record[9];
 if(phase==OTA_PENDING){
  uint8_t valid=ota_slot_valid(candidate,scratch);BOOT_TRACE(4,valid);if(!valid){candidate=active;phase=OTA_CONFIRMED;}
  if(!install(candidate)){BOOT_TRACE(5,0);return 0;}BOOT_TRACE(5,1);
  ota_record(record,ota_u32(record+4)+1,active,phase==OTA_CONFIRMED?OTA_NONE:candidate,phase==OTA_CONFIRMED?OTA_CONFIRMED:OTA_TRIAL);
  if(!ota_append_sync(record,last)){BOOT_TRACE(6,0);return 0;}BOOT_TRACE(6,1);
 }else if(phase==OTA_TRIAL){
  if(!install(active))return 0;
  ota_record(record,ota_u32(record+4)+1,active,OTA_NONE,OTA_CONFIRMED);
  if(!ota_append_sync(record,last))return 0;
 }else{
  if(!ota_slot_valid(active,scratch))return 0;
  if(!ota_flash_read((uint32_t)active*OTA_SLOT_SIZE,scratch,64))return 0;
  {uint32_t want=ota_u32(scratch+20);if(pic_crc()!=want&&!install(active))return 0;}
  if(record[11]>=2&&ota_slot_valid(active^1u,scratch)){
   ota_record(record,ota_u32(record+4)+1,active,active^1u,OTA_PENDING);
   if(!ota_append_sync(record,last))return 0;boot_reboot();return 0;
  }
  ota_put32(record+4,ota_u32(record+4)+1);if(record[11]<3)++record[11];
  ota_put32(record+24,~ota_crc(0xfffffffful,record,24));
  if(!ota_append_sync(record,last))return 0;
 }
 return 1;
}
