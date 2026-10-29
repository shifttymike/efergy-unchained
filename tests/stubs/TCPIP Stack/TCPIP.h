#ifndef TEST_TCPIP_H
#define TEST_TCPIP_H
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uintptr_t DWORD;
typedef uint8_t BYTE,TCP_SOCKET;
typedef uint16_t WORD;
typedef union {uint32_t Val;uint8_t v[4];} IP_ADDR;
typedef struct {uint8_t v[6];} MAC_ADDR;
typedef struct {
    IP_ADDR MyIPAddr,MyMask,MyGateway,PrimaryDNSServer,DefaultIPAddr,DefaultMask;
    uint8_t NetBIOSName[16];
    struct {uint8_t bIsDHCPEnabled,bInConfigMode;} Flags;
    MAC_ADDR MyMACAddr;
} APP_CONFIG;
typedef struct {MAC_ADDR SenderMACAddr;IP_ADDR SenderIPAddr,TargetIPAddr;} ARP_PACKET;
extern APP_CONFIG AppConfig;
#define PTR_BASE uintptr_t
#define INVALID_SOCKET 255
#define TCP_OPEN_SERVER 0
#define TCP_PURPOSE_HTTP 1
#define TCP_OPEN_RAM_HOST 4
#define TCP_OPEN_IP_ADDRESS 3
#define TCP_PURPOSE_MQTT 0
#define DNS_TYPE_A 1
#define TICK_SECOND 1000ul
#define TRUE 1
#define FALSE 0
#define ARP_OPERATION_REQ 1
DWORD TickGet(void);
TCP_SOCKET TCPOpen(DWORD host,BYTE type,WORD port,BYTE purpose);
void TCPClose(TCP_SOCKET socket);
void TCPDisconnect(TCP_SOCKET socket);
uint8_t TCPWasReset(TCP_SOCKET socket);
uint8_t TCPIsConnected(TCP_SOCKET socket);
WORD TCPIsPutReady(TCP_SOCKET socket);
WORD TCPPutArray(TCP_SOCKET socket,BYTE *data,WORD length);
void TCPFlush(TCP_SOCKET socket);
uint8_t TCPGet(TCP_SOCKET socket,BYTE *byte);
uint8_t StringToIPAddress(BYTE *host,IP_ADDR *address);
BYTE EfergyTCPState(TCP_SOCKET socket);
uint8_t DNSBeginUsage(void);
void DNSResolve(BYTE *host,BYTE type);
uint8_t DNSIsResolved(IP_ADDR *address);
uint8_t DNSEndUsage(void);
void StackInit(void);
uint8_t MACIsLinked(void);
uint8_t MACIsTxReady(void);
void DHCPDisable(BYTE interface);
void DHCPInit(BYTE interface);
void DHCPEnable(BYTE interface);
uint8_t DHCPIsBound(BYTE interface);
uint8_t ARPSendPkt(DWORD source,DWORD destination,BYTE operation);
#endif
