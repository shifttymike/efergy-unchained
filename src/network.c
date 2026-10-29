#include "TCPIP Stack/TCPIP.h"
#include "app.h"
#include "config_store.h"
#include "network.h"
/* 0 DHCP wait, 1 DHCP bound, 2 probing static, 3 static active, 4 conflict. */
uint8_t network_state,network_fallback;
static uint8_t boot_dhcp,gateway[4],dns[4],link_seen,probes,conflict;
static DWORD started,probe_time;
static void begin_probe(void)
{
    DHCPDisable(0);AppConfig.MyIPAddr.Val=0;AppConfig.MyMask=AppConfig.DefaultMask;
    memcpy(AppConfig.MyGateway.v,gateway,4);memcpy(AppConfig.PrimaryDNSServer.v,dns,4);
    AppConfig.Flags.bInConfigMode=TRUE;network_state=2;app_network_ready=0;
    probes=0;conflict=0;probe_time=TickGet();
}
void network_init(void)
{
    network_state=0;network_fallback=0;link_seen=0;probes=0;conflict=0;app_network_ready=0;
    boot_dhcp=device_config.dhcp;
    memcpy(AppConfig.DefaultIPAddr.v,device_config.ip,4);memcpy(AppConfig.DefaultMask.v,device_config.mask,4);
    memcpy(gateway,device_config.gateway,4);memcpy(dns,device_config.dns,4);
    AppConfig.MyIPAddr.Val=0;AppConfig.MyMask=AppConfig.DefaultMask;
    memcpy(AppConfig.MyGateway.v,gateway,4);memcpy(AppConfig.PrimaryDNSServer.v,dns,4);
    memset(AppConfig.NetBIOSName,' ',sizeof(AppConfig.NetBIOSName));
    memcpy(AppConfig.NetBIOSName,EFERGY_DHCP_HOST_NAME,sizeof(EFERGY_DHCP_HOST_NAME)-1u);
    AppConfig.Flags.bIsDHCPEnabled=boot_dhcp;AppConfig.Flags.bInConfigMode=TRUE;
    StackInit();AppConfig.Flags.bIsDHCPEnabled=boot_dhcp;
    started=TickGet();if(!boot_dhcp)begin_probe();
}
uint8_t network_config_pending(void)
{
    return boot_dhcp!=device_config.dhcp || memcmp(AppConfig.DefaultIPAddr.v,device_config.ip,4) ||
        memcmp(AppConfig.DefaultMask.v,device_config.mask,4) || memcmp(gateway,device_config.gateway,4) || memcmp(dns,device_config.dns,4);
}
/* Small vendor-stack observation hook; no extra ARP cache or frame buffer. */
void AppARPObserve(const ARP_PACKET *packet)
{
    if((network_state==2 || network_state==3) &&
       memcmp(packet->SenderMACAddr.v,AppConfig.MyMACAddr.v,6) &&
       (packet->SenderIPAddr.Val==AppConfig.DefaultIPAddr.Val ||
        (network_state==2 && !packet->SenderIPAddr.Val && packet->TargetIPAddr.Val==AppConfig.DefaultIPAddr.Val)))conflict=1;
}
void network_task(void)
{
    DWORD now=TickGet();
    if(!MACIsLinked()) {app_network_ready=0;link_seen=0;started=now;return;}
    if(!link_seen) {
        link_seen=1;started=now;network_fallback=0;
        if(boot_dhcp) {
            AppConfig.MyIPAddr.Val=0;AppConfig.Flags.bInConfigMode=TRUE;
            DHCPInit(0);DHCPEnable(0);network_state=0;
        }else begin_probe();
    }
    if((network_state==2 || network_state==3) && conflict) {
        AppConfig.MyIPAddr.Val=0;AppConfig.Flags.bInConfigMode=TRUE;
        network_state=4;app_network_ready=0;started=now;return;
    }
    if(network_state==0) {
        if(DHCPIsBound(0)){network_state=1;app_network_ready=1;return;}
        AppConfig.MyIPAddr.Val=0;
        if((DWORD)(now-started)>(DWORD)(30ul*TICK_SECOND))begin_probe();
        return;
    }
    if(network_state==1) {
        if(DHCPIsBound(0)){app_network_ready=1;return;}
        AppConfig.MyIPAddr.Val=0;AppConfig.Flags.bInConfigMode=TRUE;
        network_state=0;app_network_ready=0;started=now;return;
    }
    if(network_state==2 && (DWORD)(now-probe_time)>(DWORD)TICK_SECOND && MACIsTxReady()) {
        if(probes<3){ARPSendPkt(0,AppConfig.DefaultIPAddr.Val,ARP_OPERATION_REQ);++probes;probe_time=now;return;}
        AppConfig.MyIPAddr=AppConfig.DefaultIPAddr;AppConfig.Flags.bInConfigMode=FALSE;
        network_state=3;network_fallback=boot_dhcp;app_network_ready=1;
        ARPSendPkt(AppConfig.MyIPAddr.Val,AppConfig.MyIPAddr.Val,ARP_OPERATION_REQ);return;
    }
    if(network_state==4 && (DWORD)(now-started)>(DWORD)(60ul*TICK_SECOND)) {
        if(boot_dhcp){DHCPEnable(0);network_state=0;started=now;}else begin_probe();
    }
}
