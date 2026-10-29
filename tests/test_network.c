#include <assert.h>
#include "TCPIP Stack/TCPIP.h"
#include "config_store.h"
#include "network.h"
APP_CONFIG AppConfig={.MyMACAddr={{0x54,0x10,0xec,0xa5,0xa9,0x2a}}};
config_t device_config;
uint8_t app_network_ready,app_reboot_requested;
static DWORD now,source,target;
static uint8_t linked=1,bound,arp_count;
DWORD TickGet(void){return now;}
void StackInit(void){AppConfig.Flags.bIsDHCPEnabled=1;}
uint8_t MACIsLinked(void){return linked;}
uint8_t MACIsTxReady(void){return 1;}
void DHCPDisable(BYTE interface){assert(!interface);bound=0;}
void DHCPInit(BYTE interface){assert(!interface);bound=0;}
void DHCPEnable(BYTE interface){assert(!interface);}
uint8_t DHCPIsBound(BYTE interface){assert(!interface);return bound;}
uint8_t ARPSendPkt(DWORD src,DWORD dst,BYTE op){assert(op==1);source=src;target=dst;++arp_count;return 1;}
extern void AppARPObserve(const ARP_PACKET *packet);
static void probes(void)
{
    for(unsigned i=0;i<3;++i){now+=1001;network_task();assert(!app_network_ready&&source==0&&target==0x017ba8c0ul);}
    now+=1001;network_task();assert(app_network_ready&&network_state==3&&source==target);
}
int main(void)
{
    ARP_PACKET conflict={.SenderMACAddr={{1,2,3,4,5,6}},.SenderIPAddr={.Val=0x017ba8c0ul}};
    config_defaults(&device_config);network_init();network_task();assert(network_state==0&&!app_network_ready);
    assert(!memcmp(AppConfig.NetBIOSName,"efergyhub       ",16));
    bound=1;AppConfig.MyIPAddr.Val=0x0d17a8c0ul;network_task();assert(network_state==1&&app_network_ready);
    device_config.dhcp=0;assert(network_config_pending());network_task();assert(network_state==1&&app_network_ready);
    device_config.dhcp=1;assert(!network_config_pending());
    bound=0;network_task();assert(network_state==0&&!app_network_ready&&!AppConfig.MyIPAddr.Val);
    now+=31000;network_task();assert(network_state==2);probes();assert(network_fallback);
    AppARPObserve(&conflict);network_task();assert(network_state==4&&!app_network_ready&&!AppConfig.MyIPAddr.Val);
    now+=61000;network_task();assert(network_state==0);
    linked=0;network_task();assert(!app_network_ready);linked=1;network_task();assert(network_state==0);
    device_config.dhcp=0;arp_count=0;network_init();network_task();assert(!AppConfig.Flags.bIsDHCPEnabled);
    probes();assert(!network_fallback&&arp_count==4);
    device_config.ip[3]=8;assert(network_config_pending());
    puts("network: DHCP/lease loss, static probes, conflict withdrawal, link recovery and deferred network changes passed");
}
