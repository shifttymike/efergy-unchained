#if defined(OEM_RESTORE_BUILD) || defined(OEM_RESTORE_HOST_TEST)
#include "oem_hex.h"
static uint8_t digit(uint8_t n){return n<10?(uint8_t)('0'+n):(uint8_t)('A'+n-10);}
static uint8_t hexbyte(oem_emit_fn emit,uint8_t b){return emit(digit(b>>4))&&emit(digit(b&15));}
static uint8_t record(oem_emit_fn emit,uint8_t kind,uint16_t a,const uint8_t *p,uint8_t n){uint8_t i,sum=(uint8_t)(n+(a>>8)+a+kind);
 if(!emit(':')||!hexbyte(emit,n)||!hexbyte(emit,(uint8_t)(a>>8))||!hexbyte(emit,(uint8_t)a)||!hexbyte(emit,kind))return 0;
 for(i=0;i<n;++i){sum+=p[i];if(!hexbyte(emit,p[i]))return 0;}
 return hexbyte(emit,(uint8_t)(0u-sum))&&emit('\n');
}
uint8_t oem_hex_generate(oem_read_fn read,oem_emit_fn emit){uint32_t a;uint8_t row[16],bank=255,next[2],i,nonblank;
 for(a=0;a<0x20000ul;a+=16){
  if(a>=0x1cc00ul&&a<0x1fc00ul)continue;
  if(!read(a,row,16))return 0;
  if(!a&&!read(0x1cbc0ul,row,4))return 0;
  nonblank=0;for(i=0;i<16;++i)if(row[i]!=255)nonblank=1;
  if(!nonblank)continue;
  if(bank!=(uint8_t)(a>>16)){bank=(uint8_t)(a>>16);next[0]=0;next[1]=bank;if(!record(emit,4,0,next,2))return 0;}
  if(!record(emit,0,(uint16_t)a,row,16))return 0;
 }
 return record(emit,1,0,row,0);
}
#endif
