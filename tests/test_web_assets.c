#include <assert.h>
#include "TCPIP Stack/TCPIP.h"
#include "config.h"
#include "web_assets.h"
#include "web_assets_data.h"
static uint8_t flash[WEB_ASSET_SIZE],busy,config_busy,fail_program;
static DWORD now;
static unsigned erases,writes,commits;
DWORD TickGet(void){return now;}
uint8_t config_store_busy(void){return config_busy;}
uint32_t flash_id(void){return 0xbf258dul;}
uint8_t flash_read(uint32_t address,uint8_t *out,uint16_t n)
{assert(address>=WEB_ASSET_BASE && address+n<=WEB_ASSET_BASE+WEB_ASSET_SIZE);memcpy(out,flash+(address-WEB_ASSET_BASE),n);return 1;}
uint8_t flash_status(uint8_t *status){*status=busy?1:0;if(busy)--busy;return 1;}
uint8_t flash_erase_asset_sector(uint8_t sector)
{assert(!busy && sector<2);memset(flash+(unsigned)sector*4096,255,4096);busy=1;++erases;return 1;}
uint8_t flash_program_asset_byte(uint16_t offset,uint8_t value)
{
 assert(!busy && offset<WEB_IMAGE_SIZE);
 if(fail_program)return 0;
 if(offset==15){assert(value==0xa5 && !memcmp(flash,web_asset_image,15) && !memcmp(flash+16,web_asset_image+16,WEB_IMAGE_SIZE-16));++commits;}
 flash[offset]&=value;busy=1;++writes;return 1;
}
static void step(void){++now;web_assets_task();}
static void finish(void)
{unsigned limit=40000;while(web_assets_busy()&&--limit)step();assert(limit&&web_assets_ready()&&!memcmp(flash,web_asset_image,WEB_IMAGE_SIZE));}
int main(void)
{
 static const unsigned cuts[]={0,1,2,5,24,200,2048,4096,8192,10000};
 memset(flash,0x5a,sizeof(flash));web_assets_init();assert(!web_assets_ready());finish();
 assert(erases==2 && commits==1 && web_assets_length()==WEB_GZIP_LENGTH);
 unsigned old_erases=erases,old_writes=writes;web_assets_init();finish();assert(erases==old_erases&&writes==old_writes);
 for(unsigned cut=0;cut<sizeof(cuts)/sizeof(cuts[0]);++cut){
  memset(flash,0x5a,sizeof(flash));busy=0;web_assets_init();
  for(unsigned i=0;i<cuts[cut]&&web_assets_busy();++i)step();
  busy=0;web_assets_init();finish();
 }
 flash[15]=0x85;web_assets_init();finish(); /* Interrupted commit is repaired. */
 flash[WEB_IMAGE_SIZE-1]^=1;web_assets_init();finish(); /* Corrupt asset is repaired. */
 memset(flash,255,sizeof(flash));busy=0;web_assets_init();config_busy=1;
 old_erases=erases;for(unsigned i=0;i<100;++i)step();assert(erases==old_erases&&!web_assets_ready());config_busy=0;finish();
 memset(flash,255,sizeof(flash));busy=0;web_assets_init();fail_program=1;
 for(unsigned i=0;i<100&&web_assets_busy();++i)step();assert(web_assets_state==255&&!web_assets_ready());fail_program=0;
 busy=0;web_assets_init();step();busy=2;now+=1001;web_assets_task();assert(web_assets_state==255);
 puts("web assets: two-sector bounds, commit-last, reboot reuse, interrupted install, corruption repair, lock and timeout passed");
}
