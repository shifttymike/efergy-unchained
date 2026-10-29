#include <string.h>
#ifdef EFERGY_HOST_TEST
#include <stdint.h>
typedef uint32_t DWORD;
extern uint32_t test_tick(void);
#define TickGet test_tick
#define TICK_SECOND 1000ul
#else
#include "TCPIP Stack/TCPIP.h"
#endif
#include "board.h"
#include "config_store.h"
config_t device_config;
uint32_t config_sequence;
uint8_t config_store_state,config_store_error,config_generation;
static uint8_t record[CONFIG_RECORD_SIZE],active_slot=255,target_slot;
static uint16_t offset;
static DWORD operation_started;
static uint32_t slot(uint8_t n) {return n?CONFIG_SLOT_B:CONFIG_SLOT_A;}
uint8_t config_store_busy(void) {return config_store_state!=0 && config_store_state!=255;}
void config_store_init(void)
{
    uint32_t seq;
    active_slot=255;config_sequence=0;config_store_state=0;config_store_error=0;config_generation=0;
    config_defaults(&device_config);
    if(flash_id()!=0xbf258dul){config_store_error=1;return;}
    if(flash_read(CONFIG_SLOT_A,record,sizeof(record)) && config_decode(record,&config_scratch,&seq)) {
        memcpy(&device_config,&config_scratch,sizeof(config_scratch));config_sequence=seq;active_slot=0;
    }
    if(flash_read(CONFIG_SLOT_B,record,sizeof(record)) && config_decode(record,&config_scratch,&seq) &&
       (active_slot==255 || config_sequence_newer(seq,config_sequence))) {
        memcpy(&device_config,&config_scratch,sizeof(config_scratch));config_sequence=seq;active_slot=1;
    }
    memset(record,0,sizeof(record));
    memset(&config_scratch,0,sizeof(config_scratch));
}
uint8_t config_store_begin(const config_t *candidate)
{
#if !CONFIG_WRITES_ENABLED
    (void)candidate;return 0;
#else
    if(config_store_busy() || config_store_error==1)return 0;
    if(!config_encode(record,candidate,config_sequence+1u,0))return 0;
    target_slot=active_slot==0?1:0;
    if(!flash_erase_config_sector(slot(target_slot))){config_store_state=255;config_store_error=2;return 0;}
    config_store_error=0;offset=0;config_store_state=1;operation_started=TickGet();return 1;
#endif
}
void config_store_task(void)
{
    uint8_t status,temp[32],n,i;
    uint32_t seq;
    if(!config_store_busy())return;
    if(!flash_status(&status)){config_store_error=3;goto failed;}
    if(status&1u) {
        if((DWORD)(TickGet()-operation_started)>(DWORD)(TICK_SECOND)){config_store_error=4;goto failed;}
        return;
    }
    switch(config_store_state) {
    case 1: /* Erase or previous byte finished. */
        while(offset<CONFIG_RECORD_SIZE && (offset==CONFIG_COMMIT_OFFSET || record[offset]==255))++offset;
        if(offset==CONFIG_RECORD_SIZE){offset=0;config_store_state=2;return;}
        if(!flash_program_config_byte(slot(target_slot)+offset,record[offset])){config_store_error=5;goto failed;}
        ++offset;operation_started=TickGet();return;
    case 2:
        n=(uint8_t)(CONFIG_RECORD_SIZE-offset);if(n>sizeof(temp))n=sizeof(temp);
        if(!flash_read(slot(target_slot)+offset,temp,n)){config_store_error=6;goto failed;}
        for(i=0;i<n;++i)if(temp[i]!=record[offset+i]){config_store_error=7;goto failed;}
        offset+=n;if(offset<CONFIG_RECORD_SIZE)return;
        if(!flash_program_config_byte(slot(target_slot)+CONFIG_COMMIT_OFFSET,0xa5)){config_store_error=8;goto failed;}
        operation_started=TickGet();config_store_state=3;return;
    case 3:
        if(!flash_read(slot(target_slot),record,sizeof(record)) || !config_decode(record,&config_scratch,&seq)) {
            config_store_error=9;goto failed;
        }
        memcpy(&device_config,&config_scratch,sizeof(config_scratch));config_sequence=seq;
        active_slot=target_slot;++config_generation;config_store_state=0;
        memset(record,0,sizeof(record));memset(&config_scratch,0,sizeof(config_scratch));return;
    default:config_store_error=10;
    }
failed:config_store_state=255;memset(record,0,sizeof(record));memset(&config_scratch,0,sizeof(config_scratch));
}
