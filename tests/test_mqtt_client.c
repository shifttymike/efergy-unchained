#include <assert.h>
#include <stdlib.h>
#include "TCPIP Stack/TCPIP.h"
#include "config_store.h"
#include "mqtt_client.h"
APP_CONFIG AppConfig={.MyMACAddr={{0x54,0x10,0xec,0xa5,0xa9,0x2a}}};
config_t device_config;
static DWORD now;
static uint8_t connected,rx[8],tx[768];
static uint16_t rx_n,rx_p,tx_n;
DWORD TickGet(void){return now;}
uint8_t StringToIPAddress(BYTE *host,IP_ADDR *address){(void)host;(void)address;return 0;}
BYTE EfergyTCPState(TCP_SOCKET socket){assert(!socket);return 0x82;}
uint8_t DNSBeginUsage(void){return 1;}
void DNSResolve(BYTE *host,BYTE type){assert(!strcmp((char *)host,"broker.local")&&type==1);}
uint8_t DNSIsResolved(IP_ADDR *address){address->Val=0x1617a8c0ul;return 1;}
uint8_t DNSEndUsage(void){return 1;}
TCP_SOCKET TCPOpen(DWORD host,BYTE type,WORD port,BYTE purpose)
{
    assert(host==0x1617a8c0ul);assert(type==TCP_OPEN_IP_ADDRESS && port==1883 && !purpose);return 0;
}
void TCPClose(TCP_SOCKET socket){assert(!socket);connected=0;}
uint8_t TCPWasReset(TCP_SOCKET socket){assert(!socket);return 0;}
uint8_t TCPIsConnected(TCP_SOCKET socket){assert(!socket);return connected;}
WORD TCPIsPutReady(TCP_SOCKET socket){assert(!socket);return sizeof(tx);}
WORD TCPPutArray(TCP_SOCKET socket,BYTE *data,WORD n){assert(!socket&&n<=sizeof(tx));memcpy(tx,data,n);tx_n=n;return n;}
void TCPFlush(TCP_SOCKET socket){assert(!socket);}
uint8_t TCPGet(TCP_SOCKET socket,BYTE *byte){assert(!socket);if(rx_p==rx_n)return 0;*byte=rx[rx_p++];return 1;}
static void receive(const uint8_t *data,uint16_t n){memcpy(rx,data,n);rx_n=n;rx_p=0;}
static const char *publish_payload(const char *expected)
{
    uint16_t pos=1,remaining=0,scale=1,n;uint8_t b;
    assert(tx[0]==0x31);
    do{b=tx[pos++];remaining+=(b&127u)*scale;scale*=128;}while(b&128u);
    assert(pos+remaining==tx_n);
    n=(uint16_t)((tx[pos]<<8)|tx[pos+1]);pos+=2;
    assert(n==strlen(expected)&&!memcmp(tx+pos,expected,n));pos+=n;
    assert(tx_n<sizeof(tx));tx[tx_n]=0;return (const char *)tx+pos;
}
static void write_json(const char *file,const char *text)
{
    FILE *f=fopen(file,"w");assert(f);fputs(text,f);fclose(f);
}
int main(void)
{
    const char *payload;
    const uint8_t ack1[]={0x20,2},ack2[]={0,0},pong1[]={0xd0},pong2[]={0};
    reading_t reading={827781,34209,0x7892,13,6,1,0,169};
    config_defaults(&device_config);strcpy(device_config.host,"broker.local");
    strcpy(device_config.username,"u");strcpy(device_config.password,"p");
    mqtt_init();mqtt_reading(&reading);now=2000;mqtt_task(1);assert(mqtt_state==5);
    mqtt_task(1);assert(mqtt_state==1);
    connected=1;mqtt_task(1);assert(mqtt_state==3&&tx[0]==0x10);
    receive(ack1,2);mqtt_task(1);assert(mqtt_state==3);
    receive(ack2,2);mqtt_task(1);assert(mqtt_state==4);
    assert(!strcmp(publish_payload("efergy/5410eca5a92a/availability"),"online"));
    mqtt_task(1);payload=publish_payload("homeassistant/sensor/efergy_5410eca5a92a_827781_current/config");
    assert(strstr(payload,"\"expire_after\":30")&&strstr(payload,"\"device_class\":\"current\""));
    write_json("build/ha-current-test.json",payload);
    mqtt_task(1);payload=publish_payload("homeassistant/binary_sensor/efergy_5410eca5a92a_827781_battery/config");
    assert(strstr(payload,"value_json.battery_ok"));write_json("build/ha-battery-test.json",payload);
    mqtt_task(1);payload=publish_payload("homeassistant/sensor/efergy_5410eca5a92a_827781_rssi/config");
    assert(strstr(payload,"\"device_class\":\"signal_strength\"")&&strstr(payload,"\"unit_of_measurement\":\"dBm\""));
    write_json("build/ha-rssi-test.json",payload);
    mqtt_task(1);payload=publish_payload("homeassistant/sensor/efergy_5410eca5a92a_827781_power/config");
    assert(strstr(payload,"Estimated power")&&strstr(payload,"\"unit_of_measurement\":\"W\""));
    write_json("build/ha-power-test.json",payload);
    mqtt_task(1);payload=publish_payload("homeassistant/sensor/efergy_5410eca5a92a_827781_voltage/config");
    assert(strstr(payload,"Assumed voltage")&&strstr(payload,"\"unit_of_measurement\":\"V\""));
    write_json("build/ha-voltage-test.json",payload);
    mqtt_task(1);payload=publish_payload("homeassistant/sensor/efergy_5410eca5a92a_827781_interval/config");
    assert(strstr(payload,"\"device_class\":\"duration\"")&&strstr(payload,"value_json.interval"));
    assert(strstr(payload,"Efergy gateway"));write_json("build/ha-interval-test.json",payload);
    mqtt_task(1);payload=publish_payload("efergy/5410eca5a92a/827781/state");
    assert(strstr(payload,"\"current_a\":3.768")&&strstr(payload,"\"battery_ok\":true"));
    assert(strstr(payload,"\"power_w\":866.6") && strstr(payload,"\"voltage_v\":230") && strstr(payload,"\"rssi_dbm\":-84.5"));
    write_json("build/mqtt-state-test.json",payload);
    device_config.voltage_v=240;mqtt_reading(&reading);mqtt_task(1);
    payload=publish_payload("efergy/5410eca5a92a/827781/state");assert(strstr(payload,"\"power_w\":904.3"));
    reading.current_mantissa=65535;reading.current_divisor_shift=7;reading.rssi_raw=254;device_config.voltage_v=500;
    mqtt_reading(&reading);mqtt_task(1);payload=publish_payload("efergy/5410eca5a92a/827781/state");
    assert(strstr(payload,"\"power_w\":255996.1") && strstr(payload,"\"rssi_dbm\":-127.0"));
    reading.rssi_raw=255;mqtt_reading(&reading);mqtt_task(1);
    payload=publish_payload("efergy/5410eca5a92a/827781/state");assert(strstr(payload,"\"rssi_dbm\":null"));
    /* Largest identifier and every discovery kind must fit the fixed packet. */
    reading.legacy_id=0xffffffffu;memset(device_config.device_name,'"',31);device_config.device_name[31]=0;mqtt_reading(&reading);
    for(unsigned i=0;i<7;++i)mqtt_task(1);
    assert(mqtt_state==4 && !mqtt_error);
    now+=16000;mqtt_task(1);assert(tx_n==2&&tx[0]==0xc0&&tx[1]==0);
    receive(pong1,1);mqtt_task(1);receive(pong2,1);mqtt_task(1);assert(mqtt_state==4);
    connected=0;mqtt_task(1);assert(mqtt_state==0&&mqtt_error==2);
    now+=2000;mqtt_task(1);assert(mqtt_state==5);mqtt_task(1);assert(mqtt_state==1);
    connected=1;mqtt_task(1);assert(mqtt_state==3);
    {const uint8_t refused[]={0x20,2,0,5};connected=0;receive(refused,4);mqtt_task(1);assert(mqtt_state==0&&mqtt_error==25);}
    for(unsigned i=1;i<5;++i){reading.legacy_id=i;mqtt_reading(&reading);}
    assert(mqtt_sensor_count==5&&mqtt_dropped_sensors==1);
    puts("MQTT client: fragmented handshake, retained discovery/state, keepalive, reconnect, auth refusal and cache bounds passed");
}
