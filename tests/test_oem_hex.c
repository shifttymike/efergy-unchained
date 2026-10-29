#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "oem_hex.h"
static uint8_t image[0x20000];static FILE *out;static unsigned count,limit=~0u;static uint8_t badread;
static uint8_t read_image(uint32_t a,uint8_t *p,uint16_t n){assert(a+n<=sizeof(image));if(badread)return 0;memcpy(p,image+a,n);return 1;}
static uint8_t emit(uint8_t b){if(count++>=limit)return 0;return fputc(b,out)!=EOF;}
int main(int argc,char **argv){assert(argc==3);FILE *f=fopen(argv[1],"rb");assert(f&&fread(image,1,sizeof(image),f)==sizeof(image));fclose(f);out=fopen(argv[2],"wb");assert(out);assert(oem_hex_generate(read_image,emit));fclose(out);
 out=tmpfile();assert(out);limit=50;count=0;assert(!oem_hex_generate(read_image,emit));badread=1;assert(!oem_hex_generate(read_image,emit));fclose(out);return 0;
}
