#include "TCPIP Stack/TCPIP.h"
#include "board.h"
#include "config_store.h"
#include "web_assets.h"
#include "web_assets_data.h"
#define WEB_COMMIT 15u
uint8_t web_assets_state;
static uint8_t erase_sector;
static uint16_t position;
static DWORD operation_started;
uint8_t web_assets_ready(void){return web_assets_state==0;}
uint8_t web_assets_busy(void){return web_assets_state!=0 && web_assets_state!=255;}
uint16_t web_assets_length(void){return WEB_GZIP_LENGTH;}
void web_assets_init(void)
{
    position=0;erase_sector=0;
    web_assets_state=flash_id()==0xbf258dul?1:255;
}
void web_assets_task(void)
{
    uint8_t temp[32],n,i,status;
    if(!web_assets_busy() || config_store_busy())return;
    if(web_assets_state==1 || web_assets_state==4) {
        n=(uint8_t)((WEB_IMAGE_SIZE-position)>sizeof(temp)?sizeof(temp):(WEB_IMAGE_SIZE-position));
        if(!flash_read(WEB_ASSET_BASE+position,temp,n))goto failed;
        for(i=0;i<n;++i) {
            if(web_assets_state==4 && position+i==WEB_COMMIT)continue;
            if(temp[i]!=web_asset_image[position+i]) {
                if(web_assets_state==4)goto failed;
                erase_sector=0;
                if(!flash_erase_asset_sector(erase_sector))goto failed;
                operation_started=TickGet();web_assets_state=2;return;
            }
        }
        position+=n;
        if(position<WEB_IMAGE_SIZE)return;
        if(web_assets_state==1){web_assets_state=0;return;}
        if(!flash_program_asset_byte(WEB_COMMIT,0xa5))goto failed;
        operation_started=TickGet();web_assets_state=5;return;
    }
    if(!flash_status(&status))goto failed;
    if(status&1u) {
        if((DWORD)(TickGet()-operation_started)>(DWORD)TICK_SECOND)goto failed;
        return;
    }
    if(web_assets_state==2) {
        if(++erase_sector<WEB_ASSET_SIZE/WEB_ASSET_SECTOR_SIZE) {
            if(!flash_erase_asset_sector(erase_sector))goto failed;
            operation_started=TickGet();return;
        }
        position=0;web_assets_state=3;
    }
    if(web_assets_state==3) {
        while(position<WEB_IMAGE_SIZE && (position==WEB_COMMIT || web_asset_image[position]==255))++position;
        if(position==WEB_IMAGE_SIZE){position=0;web_assets_state=4;return;}
        if(!flash_program_asset_byte(position,web_asset_image[position]))goto failed;
        ++position;operation_started=TickGet();return;
    }
    if(web_assets_state==5) {
        if(!flash_read(WEB_ASSET_BASE+WEB_COMMIT,temp,1) || temp[0]!=0xa5)goto failed;
        web_assets_state=0;return;
    }
    goto failed;
failed:web_assets_state=255;
}
