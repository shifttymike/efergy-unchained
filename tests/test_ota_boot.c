#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <setjmp.h>
#include "ota_store.h"
#include "boot_platform.h"
static uint8_t flash[0x80000],pic[0x20000],saved_flash[0x80000],saved_pic[0x20000],image[OTA_APP_SIZE+64],r[32];
static jmp_buf cut;
static long operations,cut_at=-1;
static void mutate(void){if(operations++==cut_at)longjmp(cut,1);}
uint8_t ota_flash_read(uint32_t a,uint8_t *p,uint16_t n){assert(a+n<=0x3c000);memcpy(p,flash+a,n);return 1;}
uint8_t ota_flash_program(uint32_t a,uint8_t v){assert(a<0x3c000);assert((flash[a]&v)==v);flash[a]&=v;mutate();return 1;}
uint8_t ota_flash_erase(uint32_t a){assert(a<0x3c000&&!(a&4095));memset(flash+a,255,4096);mutate();return 1;}
uint8_t ota_flash_wait(void){return 1;}
uint8_t ota_flash_ready(void){return 1;}
void boot_pic_read(uint32_t a,uint8_t *p,uint8_t n){assert(a>=OTA_APP_BASE&&a+n<=OTA_BOOT_BASE);memcpy(p,pic+a,n);}
uint8_t boot_pic_row(uint32_t a,const uint8_t *p){assert(a>=OTA_APP_BASE&&a+64<=OTA_BOOT_BASE&&!(a&63));if(!(a&1023)){memset(pic+a,255,1024);mutate();}for(unsigned i=0;i<64;i++){assert((pic[a+i]&p[i])==p[i]);pic[a+i]&=p[i];}mutate();return 1;}
void boot_reboot(void){longjmp(cut,2);}
static uint32_t full_crc(const uint8_t *p){uint32_t c=0xfffffffful;for(unsigned a=0;a<OTA_APP_SIZE;a+=64)c=ota_crc(c,p+a,64);return ~c;}
static uint32_t current_crc(void){return full_crc(pic+OTA_APP_BASE);}
static void recover(void){cut_at=-1;for(unsigned i=0;i<4;i++){if(setjmp(cut)==0){assert(boot_run());return;}}assert(0);}
static void check(void){uint8_t temp[64];uint32_t a;assert(ota_find_record(r,&a));uint8_t slot=r[10]==OTA_TRIAL?r[9]:r[8];assert(ota_slot_valid(slot,temp));assert(current_crc()==ota_u32(flash+(uint32_t)slot*OTA_SLOT_SIZE+20));for(unsigned i=0;i<OTA_APP_BASE;i++)assert(pic[i]==0x69);for(unsigned i=OTA_BOOT_BASE;i<sizeof(pic);i++)assert(pic[i]==0x69);for(unsigned i=0x3c000;i<sizeof(flash);i++)assert(flash[i]==255);}
int main(void){FILE *f=fopen("build/efergy-update.efu","rb");assert(f);assert(fread(image,1,sizeof(image),f)==sizeof(image));fclose(f);assert(ota_header_valid(image));
 memset(flash,255,sizeof(flash));memset(pic,0x69,sizeof(pic));memcpy(pic+OTA_APP_BASE,image+64,OTA_APP_SIZE);
 recover();check(); /* Initial factory snapshot with actual linked application. */
 long seed_total=operations;unsigned seed_tests=0;
 memcpy(saved_pic,pic,sizeof(pic));
 for(long c=0;c<seed_total;c+=(c>seed_total-65?1:8192)){
  memset(flash,255,sizeof(flash));memcpy(pic,saved_pic,sizeof(pic));operations=0;cut_at=c;
  if(setjmp(cut)==0)(void)boot_run();recover();check();++seed_tests;
 }
 printf("Initial snapshot: %u interrupted seed/commit boundaries recovered\n",seed_tests);
 memcpy(saved_flash,flash,sizeof(flash));memcpy(saved_pic,pic,sizeof(pic));
 unsigned tests=0;
 /* Build a different valid candidate. */
 for(unsigned i=64;i<sizeof(image);i++)image[i]^=0x5a;
 ota_put32(image+20,full_crc(image+64));ota_put32(image+56,~ota_crc(0xfffffffful,image,56));
 memcpy(flash+OTA_SLOT_SIZE,image,sizeof(image));uint32_t a;assert(ota_find_record(r,&a));ota_record(r,ota_u32(r+4)+1,0,1,OTA_PENDING);assert(ota_append_sync(r,a));
 memcpy(saved_flash,flash,sizeof(flash));operations=0;recover();long total=operations;check();assert(current_crc()==ota_u32(flash+OTA_SLOT_SIZE+20));
 /* Power loss after representative row erases/writes and every journal tail byte. */
 for(long c=0;c<total;c+=(c>total-70?1:17)){
  memcpy(flash,saved_flash,sizeof(flash));memcpy(pic,saved_pic,sizeof(pic));operations=0;cut_at=c;
  if(setjmp(cut)==0)(void)boot_run();recover();check();++tests;
 }
 /* Unconfirmed trial reset restores the previous image. */
 memcpy(flash,saved_flash,sizeof(flash));memcpy(pic,saved_pic,sizeof(pic));recover();recover();check();assert(current_crc()==ota_u32(flash+20));
 /* Corrupt pending payload is rejected and the active image restored. */
 memcpy(flash,saved_flash,sizeof(flash));memcpy(pic,saved_pic,sizeof(pic));flash[OTA_SLOT_SIZE+150]^=1;recover();check();assert(current_crc()==ota_u32(flash+20));
 /* Three short confirmed boots request the alternate valid image. */
 memcpy(flash,saved_flash,sizeof(flash));memcpy(pic,saved_pic,sizeof(pic));assert(ota_find_record(r,&a));ota_record(r,ota_u32(r+4)+1,0,OTA_NONE,OTA_CONFIRMED);r[11]=2;ota_put32(r+24,~ota_crc(0xfffffffful,r,24));assert(ota_append_sync(r,a));recover();check();assert(current_crc()==ota_u32(flash+OTA_SLOT_SIZE+20));
 /* Every erase/program boundary of journal sector rotation. */
 memset(flash+OTA_JOURNAL_A,255,8192);
 for(unsigned j=0;j<128;++j){ota_record(r,j+1,0,OTA_NONE,OTA_CONFIRMED);memcpy(flash+OTA_JOURNAL_A+j*32,r,32);}
 memcpy(saved_flash,flash,sizeof(flash));
 for(long c=0;c<33;++c){
  memcpy(flash,saved_flash,sizeof(flash));ota_record(r,129,0,1,OTA_PENDING);operations=0;cut_at=c;
  if(setjmp(cut)==0)(void)ota_append_sync(r,OTA_JOURNAL_A+127*32);
  cut_at=-1;assert(ota_find_record(r,&a));assert(ota_u32(r+4)==128||ota_u32(r+4)==129);
  if(ota_u32(r+4)==128){ota_record(r,129,0,1,OTA_PENDING);assert(ota_append_sync(r,a));assert(ota_find_record(r,&a)&&ota_u32(r+4)==129);}
 }
 printf("Journal rotation: all 33 interrupted erase/program boundaries retained a valid record\n");
 printf("Boot flow: %u interrupted installs, fixed-region preservation, initial snapshot, trial rollback, bad-image rejection and power-cycle recovery passed\n",tests);
}
