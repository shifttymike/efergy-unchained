#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "config_store.h"
#include "legacy_config.h"
static uint8_t flash[8192],baseline[8192],busy,fail_write;
static uint32_t ticks;
uint32_t test_tick(void){return ticks;}
uint32_t flash_id(void){return 0xbf258dul;}
uint8_t flash_read(uint32_t address,uint8_t *out,uint16_t n)
{
    assert(address>=CONFIG_SLOT_A && address+n<=CONFIG_SLOT_B+4096ul);
    memcpy(out,flash+(address-CONFIG_SLOT_A),n);return 1;
}
uint8_t flash_status(uint8_t *status){*status=busy?1:0;if(busy)--busy;return 1;}
uint8_t flash_erase_config_sector(uint32_t address)
{
    assert(address==CONFIG_SLOT_A||address==CONFIG_SLOT_B);
    memset(flash+(address-CONFIG_SLOT_A),255,4096);busy=1;return 1;
}
uint8_t flash_program_config_byte(uint32_t address,uint8_t value)
{
    uint32_t base=address&0xfffff000ul;
    assert((base==CONFIG_SLOT_A||base==CONFIG_SLOT_B)&&address-base<CONFIG_RECORD_SIZE);
    if(fail_write)return 0;
    flash[address-CONFIG_SLOT_A]&=value;busy=1;return 1;
}
static void finish(void)
{
    unsigned limit=1000;
    while(config_store_busy()&&--limit){++ticks;config_store_task();}
    assert(limit && !config_store_error && config_store_state==0);
}
int main(void)
{
    config_t c;uint8_t legacy[CONFIG_RECORD_SIZE];unsigned cut,i;
    memset(flash,255,sizeof(flash));config_store_init();assert(config_sequence==0);
    config_defaults(&c);strcpy(c.host,"first.local");assert(config_store_begin(&c));finish();
    assert(config_sequence==1&&!strcmp(device_config.host,"first.local"));
    memcpy(baseline,flash,sizeof(flash));strcpy(c.host,"second.local");
    /* Simulated resets at each erase/program/verify/commit scheduling boundary. */
    for(cut=0;cut<650;++cut) {
        memcpy(flash,baseline,sizeof(flash));busy=0;config_store_init();assert(config_store_begin(&c));
        for(i=0;i<cut&&config_store_busy();++i){++ticks;config_store_task();}
        busy=0;config_store_init();
        assert((config_sequence==1&&!strcmp(device_config.host,"first.local"))||
               (config_sequence==2&&!strcmp(device_config.host,"second.local")));
    }
    memcpy(flash,baseline,sizeof(flash));busy=0;config_store_init();
    assert(config_store_begin(&c));fail_write=1;busy=0;config_store_task();fail_write=0;
    assert(config_store_error && config_sequence==1&&!strcmp(device_config.host,"first.local"));
    busy=0;config_store_init();assert(config_sequence==1);
    assert(config_store_begin(&c));ticks+=1001;busy=2;config_store_task();
    assert(config_store_error==4 && config_sequence==1);
    busy=0;config_store_init();assert(config_sequence==1);
    assert(config_store_begin(&c));finish();assert(config_sequence==2);
    strcpy(c.host,"third.local");assert(config_store_begin(&c));finish();config_store_init();
    assert(config_sequence==3&&!strcmp(device_config.host,"third.local"));
    /* A deployed v1 record must retain credentials on boot and migration. */
    memset(flash,255,sizeof(flash));busy=0;config_defaults(&c);
    strcpy(c.host,"existing.broker");strcpy(c.username,"existing-user");strcpy(c.password,"existing-password");
    make_v1(legacy,&c,14);memcpy(flash,legacy,CONFIG_RECORD_V1_SIZE);
    config_store_init();assert(config_sequence==14 && device_config.voltage_v==230);
    assert(!strcmp(device_config.password,c.password));
    c.voltage_v=240;assert(config_store_begin(&c));finish();config_store_init();
    assert(config_sequence==15 && device_config.voltage_v==240 && !strcmp(device_config.host,c.host));
    assert(!strcmp(device_config.username,c.username) && !strcmp(device_config.password,c.password));
    memset(flash,255,sizeof(flash));busy=0;make_v2(legacy,&c,16);memcpy(flash,legacy,CONFIG_RECORD_V2_SIZE);
    config_store_init();assert(config_sequence==16 && device_config.voltage_v==240 && !strcmp(device_config.device_name,"Efergy gateway"));
    strcpy(c.device_name,"Kitchen meter");assert(config_store_begin(&c));finish();config_store_init();
    assert(config_sequence==17 && !strcmp(device_config.device_name,"Kitchen meter") && !strcmp(device_config.password,c.password));
    puts("storage: 650 reset boundaries, commit ordering, alternating slots, write failure and busy timeout passed");
}
