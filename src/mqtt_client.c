#include "TCPIP Stack/TCPIP.h"
#include "config_store.h"
#include "mqtt_codec.h"
#include "mqtt_client.h"
#define SENSOR_LIMIT 5u
enum {DISC_CURRENT,DISC_BATTERY,DISC_RSSI,DISC_POWER,DISC_VOLTAGE,DISC_INTERVAL,DISC_COUNT};
typedef struct {reading_t reading;uint8_t discovery,pending;} sensor_t;
static sensor_t sensors[SENSOR_LIMIT];
#define packet (app_workspace.packet)
static char mac_id[13],client_id[24],topic[128];
static TCP_SOCKET socket=INVALID_SOCKET;
static mqtt_control_t control;
static DWORD changed,last_tx,ping_started;
static uint8_t backoff=1,awaiting_ping,online_sent;
static uint8_t dns_owned;
uint8_t mqtt_state,mqtt_error,mqtt_sensor_count;
uint8_t mqtt_tcp_phase;
uint32_t mqtt_resolved_ip;
extern BYTE EfergyTCPState(TCP_SOCKET socket);
uint16_t mqtt_reconnects,mqtt_publications,mqtt_dropped_sensors;
/* States: disabled/backoff=0, TCP connecting=1, send CONNECT=2,
 * waiting CONNACK=3, connected=4. MQTT 3.1.1, Clean Session, QoS 0. */
static void disconnect(uint8_t error)
{
    if(dns_owned){DNSEndUsage();dns_owned=0;}
    if(socket!=INVALID_SOCKET)TCPClose(socket);
    socket=INVALID_SOCKET;mqtt_tcp_phase=0;memset(&control,0,sizeof(control));awaiting_ping=0;online_sent=0;
    mqtt_error=error;mqtt_state=0;changed=TickGet();
    if(backoff<32)backoff*=2;else backoff=60;
}
void mqtt_init(void)
{
    uint8_t i;static const char digits[]="0123456789abcdef";
    for(i=0;i<6;++i){mac_id[i*2]=digits[AppConfig.MyMACAddr.v[i]>>4];mac_id[i*2+1]=digits[AppConfig.MyMACAddr.v[i]&15u];}
    mac_id[12]=0;snprintf(client_id,sizeof(client_id),"efergy-%s",mac_id);
    changed=TickGet();
}
void mqtt_reconfigure(void)
{
    /* Force reconnection so changed credentials and Last Will apply together.
     * Closing TCP lets the previous session's retained Will mark it offline. */
    disconnect(0);backoff=1;
}
void mqtt_reading(const reading_t *reading)
{
    uint8_t i;
    for(i=0;i<mqtt_sensor_count;++i)if(sensors[i].reading.legacy_id==reading->legacy_id)break;
    if(i==mqtt_sensor_count) {
        if(i==SENSOR_LIMIT){++mqtt_dropped_sensors;return;}
        ++mqtt_sensor_count;sensors[i].discovery=0;
    }
    if(sensors[i].reading.interval_s!=reading->interval_s)sensors[i].discovery=0;
    memcpy(&sensors[i].reading,reading,sizeof(*reading));sensors[i].pending=1;
}
static uint8_t send_packet(uint16_t n)
{
    if(!n || n>sizeof(packet)){disconnect(8);return 0;}
    if(TCPIsPutReady(socket)<n)return 0;
    if(TCPPutArray(socket,packet,n)!=n){disconnect(9);return 0;}
    TCPFlush(socket);last_tx=TickGet();return 1;
}
static uint8_t publish(const char *payload,uint16_t length)
{
    uint16_t n=mqtt_publish_packet(packet,sizeof(packet),topic,(const uint8_t *)payload,length,1);
    if(send_packet(n)){++mqtt_publications;return 1;}return 0;
}
static uint8_t discovery(sensor_t *s,uint8_t kind)
{
    char *body=(char *)packet+160;
    char device_name[CONFIG_NAME_SIZE*2u];
    uint16_t n,expiry=(uint16_t)s->reading.interval_s*5u;
    const char *name="Current",*suffix="current",*device_class="current";
    const char *extra="\"unit_of_measurement\":\"A\",\"state_class\":\"measurement\",";
    const char *value="{{ value_json.current_a }}";
    switch(kind) {
    case DISC_BATTERY:
        name="Battery";suffix="battery";device_class="battery";extra="";
        value="{{ 'OFF' if value_json.battery_ok else 'ON' }}";break;
    case DISC_RSSI:
        name="Signal strength";suffix="rssi";device_class="signal_strength";
        extra="\"unit_of_measurement\":\"dBm\",\"state_class\":\"measurement\",\"entity_category\":\"diagnostic\",";
        value="{{ value_json.rssi_dbm }}";break;
    case DISC_POWER:
        name="Estimated power";suffix="power";device_class="power";
        extra="\"unit_of_measurement\":\"W\",\"state_class\":\"measurement\",";
        value="{{ value_json.power_w }}";break;
    case DISC_VOLTAGE:
        name="Assumed voltage";suffix="voltage";device_class="voltage";
        extra="\"unit_of_measurement\":\"V\",\"entity_category\":\"diagnostic\",";
        value="{{ value_json.voltage_v }}";break;
    case DISC_INTERVAL:
        name="Reporting interval";suffix="interval";device_class="duration";
        extra="\"unit_of_measurement\":\"s\",\"entity_category\":\"diagnostic\",";
        value="{{ value_json.interval }}";break;
    default:break;
    }
    if(expiry<30)expiry=30;
    if(!config_name_json(device_name,sizeof(device_name),&device_config)){disconnect(8);return 0;}
    snprintf(topic,sizeof(topic),"homeassistant/%s/efergy_%s_%lu_%s/config",kind==DISC_BATTERY?"binary_sensor":"sensor",mac_id,(unsigned long)s->reading.legacy_id,suffix);
    n=(uint16_t)snprintf(body,sizeof(packet)-160,
        "{\"name\":\"%s\",\"unique_id\":\"efergy_%s_%lu_%s\","
        "\"state_topic\":\"efergy/%s/%lu/state\",\"availability_topic\":\"efergy/%s/availability\","
        "\"expire_after\":%u,\"device_class\":\"%s\",%s"
        "\"value_template\":\"%s\",\"device\":{\"identifiers\":[\"efergy_%s_%lu\"],"
        "\"name\":\"%s\",\"manufacturer\":\"Efergy\",\"model\":\"e2 classic\"}}",
        name,mac_id,(unsigned long)s->reading.legacy_id,suffix,
        mac_id,(unsigned long)s->reading.legacy_id,mac_id,expiry,device_class,extra,value,
        mac_id,(unsigned long)s->reading.legacy_id,device_name);
    if(n>=sizeof(packet)-160){disconnect(8);return 0;}
    return publish(body,n);
}
static uint8_t state(sensor_t *s)
{
    char *body=(char *)packet+160;
    uint32_t ma=((uint32_t)s->reading.current_mantissa*1000ul+
        (1ul<<(s->reading.current_divisor_shift-1u)))>>s->reading.current_divisor_shift;
    uint32_t power=(((uint32_t)s->reading.current_mantissa*device_config.voltage_v*10ul)+
        (1ul<<(s->reading.current_divisor_shift-1u)))>>s->reading.current_divisor_shift;
    char rssi[8];
    uint16_t n;
    if(s->reading.rssi_raw==255)strcpy(rssi,"null");
    else snprintf(rssi,sizeof(rssi),"-%u.%u",s->reading.rssi_raw/2u,(s->reading.rssi_raw&1u)?5u:0u);
    snprintf(topic,sizeof(topic),"efergy/%s/%lu/state",mac_id,(unsigned long)s->reading.legacy_id);
    n=(uint16_t)snprintf(body,sizeof(packet)-160,
        "{\"transmitter_id\":%lu,\"current_a\":%lu.%03u,\"battery_ok\":%s,\"interval\":%u,\"learn\":%s,\"power_w\":%lu.%u,\"voltage_v\":%u,\"rssi_dbm\":%s}",
        (unsigned long)s->reading.legacy_id,(unsigned long)(ma/1000ul),(unsigned)(ma%1000ul),
        s->reading.battery_ok?"true":"false",s->reading.interval_s,s->reading.learn?"true":"false",
        (unsigned long)(power/10ul),(unsigned)(power%10ul),device_config.voltage_v,rssi);
    if(n>=sizeof(packet)-160){disconnect(8);return 0;}
    return publish(body,n);
}
void mqtt_task(uint8_t ready)
{
    DWORD now=TickGet();uint8_t byte,i;uint16_t n;IP_ADDR numeric;mqtt_event_t event;
    if(!ready || !device_config.host[0]) {if(socket!=INVALID_SOCKET || dns_owned || mqtt_state)disconnect(0);return;}
    if(mqtt_state==0) {
        if((DWORD)(now-changed)<(DWORD)((DWORD)backoff*TICK_SECOND))return;
        if(!StringToIPAddress((BYTE *)device_config.host,&numeric)) {
            if(!DNSBeginUsage())return;
            dns_owned=1;mqtt_resolved_ip=0;DNSResolve((BYTE *)device_config.host,DNS_TYPE_A);
            mqtt_state=5;changed=now;return;
        }
        mqtt_resolved_ip=numeric.Val;
        socket=TCPOpen(numeric.Val,TCP_OPEN_IP_ADDRESS,device_config.port,TCP_PURPOSE_MQTT);
        if(socket==INVALID_SOCKET){changed=now;mqtt_error=1;return;}
        ++mqtt_reconnects;mqtt_state=1;changed=now;
    }
    if(mqtt_state==5) {
        if((DWORD)(now-changed)>(DWORD)(10ul*TICK_SECOND)){disconnect(7);return;}
        if(!DNSIsResolved(&numeric))return;
        dns_owned=0;
        if(!DNSEndUsage() || !numeric.Val){disconnect(7);return;}
        mqtt_resolved_ip=numeric.Val;
        socket=TCPOpen(numeric.Val,TCP_OPEN_IP_ADDRESS,device_config.port,TCP_PURPOSE_MQTT);
        if(socket==INVALID_SOCKET){mqtt_state=0;mqtt_error=1;changed=now;return;}
        ++mqtt_reconnects;mqtt_state=1;changed=now;
    }
    mqtt_tcp_phase=EfergyTCPState(socket);
    if(TCPWasReset(socket) && mqtt_state>=3){disconnect(2);return;}
    if(mqtt_state==1) {
        if(TCPIsConnected(socket)){mqtt_state=2;changed=now;}
        else if((DWORD)(now-changed)>(DWORD)(15ul*TICK_SECOND)){disconnect(3);return;}
        else return;
    }
    if(mqtt_state==2 && !TCPIsConnected(socket)){disconnect(2);return;}
    if(mqtt_state==2) {
        snprintf(topic,sizeof(topic),"efergy/%s/availability",mac_id);
        n=mqtt_connect_packet(packet,sizeof(packet),client_id,topic,device_config.username,device_config.password,30);
        if(send_packet(n)){mqtt_state=3;changed=now;}return;
    }
    while(TCPGet(socket,&byte)) {
        event=mqtt_control_feed(&control,byte);
        if(event==MQTT_MALFORMED){disconnect(4);return;}
        if(event==MQTT_REFUSED){disconnect((uint8_t)(20+control.refusal));return;}
        if(event==MQTT_CONNECTED) {
            if(mqtt_state!=3){disconnect(4);return;}
            mqtt_state=4;mqtt_error=0;backoff=1;online_sent=0;
            for(i=0;i<mqtt_sensor_count;++i){sensors[i].discovery=0;sensors[i].pending=1;}
        }
        if(event==MQTT_PONG)awaiting_ping=0;
    }
    /* A broker may send a refusal and FIN together. Drain its CONNACK before
     * treating CLOSE_WAIT as a generic connection failure. */
    if(!TCPIsConnected(socket)){disconnect(2);return;}
    if(mqtt_state==3) {if((DWORD)(now-changed)>(DWORD)(10ul*TICK_SECOND))disconnect(5);return;}
    if(awaiting_ping && (DWORD)(now-ping_started)>(DWORD)(10ul*TICK_SECOND)){disconnect(6);return;}
    if(!online_sent) {
        snprintf(topic,sizeof(topic),"efergy/%s/availability",mac_id);
        if(publish("online",6))online_sent=1;return;
    }
    if(!awaiting_ping && (DWORD)(now-last_tx)>(DWORD)(15ul*TICK_SECOND)) {
        packet[0]=0xc0;packet[1]=0;if(send_packet(2)){awaiting_ping=1;ping_started=now;}return;
    }
    for(i=0;i<mqtt_sensor_count;++i) {
        if(sensors[i].discovery<DISC_COUNT) {
            if(discovery(&sensors[i],sensors[i].discovery))++sensors[i].discovery;
            return;
        }
        if(sensors[i].pending) {if(state(&sensors[i]))sensors[i].pending=0;return;}
    }
}
