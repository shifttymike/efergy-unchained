#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "mqtt_codec.h"
#include "legacy_config.h"
int main(void)
{
    config_t config,decoded;
    uint8_t record[CONFIG_RECORD_SIZE],damaged[CONFIG_RECORD_SIZE],packet[MQTT_PACKET_MAX];
    uint32_t sequence;
    uint16_t n,i;
    mqtt_control_t parser={0};
    static const uint8_t connect[]={0x10,0x19,0,4,'M','Q','T','T',4,0x26,0,30,0,1,'c',0,1,'a',0,7,'o','f','f','l','i','n','e'};
    static const uint8_t publish[]={0x31,5,0,1,'a','h','i'};
    config_defaults(&config);assert(config_valid(&config) && config.voltage_v==230);
    strcpy(config.host,"broker.local");strcpy(config.username,"test");strcpy(config.password,"test-secret");
    assert(config_encode(record,&config,0xfffffffeul,1));
    assert(config_decode(record,&decoded,&sequence));
    assert(sequence==0xfffffffeul && decoded.port==1883 && !strcmp(decoded.password,config.password));
    assert(decoded.voltage_v==230 && record[4]==3 && record[5]==214);
    {
        uint8_t legacy[CONFIG_RECORD_V1_SIZE];
        make_v1(damaged,&config,14);memcpy(legacy,damaged,sizeof(legacy));
        assert(config_decode(legacy,&decoded,&sequence) && sequence==14 && decoded.voltage_v==230);
        assert(!strcmp(decoded.host,config.host) && !strcmp(decoded.password,config.password));
        make_v2(damaged,&config,15);
        {uint8_t v2[CONFIG_RECORD_V2_SIZE];memcpy(v2,damaged,sizeof(v2));
        assert(config_decode(v2,&decoded,&sequence) && decoded.voltage_v==230 && !strcmp(decoded.device_name,"Efergy gateway"));}
        strcpy(config.device_name,"Kitchen \"meter\" \\ 1");
        {char escaped[64];assert(config_name_json(escaped,sizeof(escaped),&config));
        assert(!strcmp(escaped,"Kitchen \\\"meter\\\" \\\\ 1"));assert(!config_name_json(escaped,3,&config));}
        config.voltage_v=240;assert(config_encode(damaged,&config,15,1));
        assert(config_decode(damaged,&decoded,&sequence) && decoded.voltage_v==240 && sequence==15 && !strcmp(decoded.device_name,config.device_name));
        config.voltage_v=0;assert(!config_valid(&config));config.voltage_v=501;assert(!config_valid(&config));config.voltage_v=230;
    }
    for(i=0;i<CONFIG_RECORD_SIZE;++i) {
        memcpy(damaged,record,sizeof(record));damaged[i]^=1;
        assert(!config_decode(damaged,&decoded,&sequence));
    }
    assert(config_encode(record,&config,0xfffffffful,0));
    assert(!config_decode(record,&decoded,&sequence));
    record[CONFIG_COMMIT_OFFSET]=0xa5;
    assert(config_decode(record,&decoded,&sequence) && sequence==0xfffffffful);
    assert(config_sequence_newer(0,0xfffffffful));
    assert(!config_sequence_newer(0xfffffffful,0));
    assert(!config_sequence_newer(12,12));
    config.mask[1]=0;assert(!config_valid(&config));config_defaults(&config);
    config.ip[3]=255;assert(!config_valid(&config));config_defaults(&config);
    memset(config.host,'x',sizeof(config.host));assert(!config_valid(&config));
    n=mqtt_connect_packet(packet,sizeof(packet),"c","a","","",30);
    assert(n==sizeof(connect) && !memcmp(packet,connect,n));
    assert(mqtt_connect_packet(packet,n,"c","a","","",30)==n);
    memset(packet,0xaa,sizeof(packet));
    assert(!mqtt_connect_packet(packet,n-1,"c","a","","",30) && packet[0]==0xaa);
    n=mqtt_connect_packet(packet,sizeof(packet),"c","a","","p",30);
    assert(n==sizeof(connect)+5 && packet[9]==0xe6);
    n=mqtt_publish_packet(packet,sizeof(packet),"a",(const uint8_t *)"hi",2,1);
    assert(n==sizeof(publish) && !memcmp(packet,publish,n));
    assert(!mqtt_publish_packet(packet,sizeof(packet),"bad/+",packet,1,0));
    memset(damaged,'x',sizeof(damaged));
    n=mqtt_publish_packet(packet,sizeof(packet),"a",damaged,127,0);
    assert(n==133 && packet[1]==0x82 && packet[2]==1);
    assert(!mqtt_publish_packet(packet,132,"a",damaged,127,0));
    assert(mqtt_control_feed(&parser,0x20)==MQTT_MORE);
    assert(mqtt_control_feed(&parser,2)==MQTT_MORE);
    assert(mqtt_control_feed(&parser,0)==MQTT_MORE);
    assert(mqtt_control_feed(&parser,0)==MQTT_CONNECTED);
    assert(mqtt_control_feed(&parser,0xd0)==MQTT_MORE);
    assert(mqtt_control_feed(&parser,0)==MQTT_PONG);
    assert(mqtt_control_feed(&parser,0x20)==MQTT_MORE);
    assert(mqtt_control_feed(&parser,0x82)==MQTT_MALFORMED);
    assert(mqtt_control_feed(&parser,0x20)==MQTT_MORE);
    assert(mqtt_control_feed(&parser,2)==MQTT_MORE);
    assert(mqtt_control_feed(&parser,1)==MQTT_MALFORMED);
    puts("protocol: config CRC/commit/wrap, bounded MQTT CONNECT/PUBLISH and fragmented control packets passed");
    return 0;
}
