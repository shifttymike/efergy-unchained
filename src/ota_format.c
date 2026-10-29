#include <string.h>
#include "ota_format.h"
uint32_t ota_u32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
void ota_put32(uint8_t *p,uint32_t v){uint8_t i;for(i=0;i<4;++i){p[i]=(uint8_t)v;v>>=8;}}
uint32_t ota_crc(uint32_t crc,const uint8_t *p,uint16_t n){uint8_t i;while(n--){crc^=*p++;for(i=0;i<8;++i)crc=(crc>>1)^((crc&1u)?0xedb88320ul:0);}return crc;}
uint8_t ota_header_valid(const uint8_t *p){return !memcmp(p,"EFGYOTA1",8)&&ota_u32(p+8)==OTA_MODEL&&ota_u32(p+12)==OTA_APP_BASE&&ota_u32(p+16)==OTA_APP_SIZE&&ota_u32(p+56)==~ota_crc(0xfffffffful,p,56)&&p[63]==0xa5;}
uint8_t ota_record_valid(const uint8_t *p){return !memcmp(p,"OTAJ",4)&&p[8]<2&&p[10]<=OTA_TRIAL&&p[11]<4&&((p[10]==OTA_CONFIRMED&&p[9]==OTA_NONE)||(p[10]!=OTA_CONFIRMED&&p[9]<2&&p[9]!=p[8]))&&ota_u32(p+24)==~ota_crc(0xfffffffful,p,24)&&p[31]==0xa5;}
void ota_record(uint8_t *p,uint32_t seq,uint8_t active,uint8_t candidate,uint8_t phase){memset(p,255,OTA_RECORD_SIZE);memcpy(p,"OTAJ",4);ota_put32(p+4,seq);p[8]=active;p[9]=candidate;p[10]=phase;p[11]=0;ota_put32(p+24,~ota_crc(0xfffffffful,p,24));p[31]=0xa5;}
