"""Compile the actual DHCP sender and inspect option 12 in its wire output."""
import re,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
source=(root/'vendor/microchip/src/DHCP.c').read_text()
start=source.index('static void _DHCPSend(BYTE messageType, BOOL bRenewing)\n{')
end=source.index('\n}',start)+2
header=(root/'vendor/microchip/include/TCPIP Stack/DHCP.h').read_text()
constants='\n'.join(line for line in header.splitlines() if re.match(r'#define\s+(BOOT_|DHCP_)',line))
prefix=r'''
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "network.h"
typedef uint8_t BYTE,BOOL;
typedef union {uint32_t Val;uint8_t v[4];} IP_ADDR;
#define ROM const
#define FALSE 0
#define MY_DEFAULT_HOST_NAME EFERGY_DHCP_HOST_NAME
static struct {IP_ADDR MyIPAddr;struct {uint8_t v[6];} MyMACAddr;} AppConfig;
static struct {IP_ADDR tempIPAddress;uint32_t dwServerID;struct {struct {uint8_t bUseUnicastMode,bOfferReceived;} bits;} flags;} DHCPClient;
static uint8_t packet[512];static unsigned UDPTxCount;
static void UDPPut(BYTE byte){assert(UDPTxCount<sizeof(packet));packet[UDPTxCount++]=byte;}
static void UDPPutArray(BYTE *data,unsigned n){for(unsigned i=0;i<n;++i)UDPPut(data[i]);}
static void UDPPutROMArray(const BYTE *data,unsigned n){for(unsigned i=0;i<n;++i)UDPPut(data[i]);}
static void UDPFlush(void){}
'''
test=r'''
int main(void){
 for(unsigned kind=0;kind<3;++kind){
  UDPTxCount=0;AppConfig.MyIPAddr.Val=0x0d17a8c0u;
  _DHCPSend(kind==0?DHCP_DISCOVER_MESSAGE:DHCP_REQUEST_MESSAGE,kind==2);
  assert(UDPTxCount>=300&&AppConfig.MyIPAddr.Val==0x0d17a8c0u);
  unsigned names=0,pos=240;
  while(packet[pos]!=255){unsigned option=packet[pos++],n=packet[pos++];assert(pos+n<=UDPTxCount);
   if(option==12){assert(n==9&&!memcmp(packet+pos,"efergyhub",9));++names;}
   pos+=n;
  }
  assert(names==1);
 }
 puts("DHCP: exact sender advertises option 12 efergyhub in DISCOVER, REQUEST and renewal packets");
}
'''
out=root/'build/test_dhcp_hostname.c';out.write_text(prefix+constants+'\n'+source[start:end]+test)
subprocess.run(['clang','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'include'),str(out),'-o',str(out.with_suffix(''))],check=True)
subprocess.run([str(out.with_suffix(''))],check=True)
