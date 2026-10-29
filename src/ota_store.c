#include <string.h>
#include "ota_store.h"
uint8_t ota_find_record(uint8_t *out,uint32_t *address){uint32_t a,seq=0;uint8_t r[32],found=0;for(a=OTA_JOURNAL_A;a<OTA_JOURNAL_B+4096ul;a+=32){if(!ota_flash_read(a,r,32))return 0;if(ota_record_valid(r)&&(!found||(int32_t)(ota_u32(r+4)-seq)>0)){memcpy(out,r,32);seq=ota_u32(r+4);*address=a;found=1;}}return found;}
uint8_t ota_next_record(uint32_t last,uint32_t *next,uint8_t *erase){uint8_t r[32],i;uint32_t a,end;
 if(last<OTA_JOURNAL_A||last>=OTA_JOURNAL_B+4096ul)return 0;
 end=(last&~4095ul)+4096ul;
 for(a=last+32;a<end;a+=32){if(!ota_flash_read(a,r,32))return 0;for(i=0;i<32&&r[i]==255;++i){}if(i==32){*next=a;*erase=0;return 1;}}
 *next=(last<OTA_JOURNAL_B)?OTA_JOURNAL_B:OTA_JOURNAL_A;*erase=1;return 1;}
uint8_t ota_append_sync(uint8_t *r,uint32_t last){uint32_t a;uint8_t erase,i,b;if(!ota_next_record(last,&a,&erase))return 0;if(erase&&(!ota_flash_erase(a)||!ota_flash_wait()))return 0;for(i=0;i<32;++i){if(!ota_flash_program(a+i,r[i])||!ota_flash_wait()||!ota_flash_read(a+i,&b,1)||b!=r[i])return 0;}return 1;}
uint8_t ota_slot_valid(uint8_t slot,uint8_t *p){uint32_t a,crc=0xfffffffful,want;if(slot>1||!ota_flash_read((uint32_t)slot*OTA_SLOT_SIZE,p,64)||!ota_header_valid(p))return 0;want=ota_u32(p+20);for(a=0;a<OTA_APP_SIZE;a+=64){if(!ota_flash_read((uint32_t)slot*OTA_SLOT_SIZE+64+a,p,64))return 0;crc=ota_crc(crc,p,64);}return ~crc==want;}
