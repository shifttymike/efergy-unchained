#include <assert.h>
#include <stdlib.h>
#include "TCPIP Stack/TCPIP.h"
#include "app.h"
#include "config_store.h"
config_t device_config;
uint32_t config_sequence;
uint8_t config_store_state,config_store_error,config_generation;
uint8_t app_network_ready,app_reboot_requested;
static const char *input="POST /reboot HTTP/1.0\r\nContent-Length: 0\r\n\r\n";
static unsigned cursor;
static uint8_t reset,connected=1;
static WORD space=400,limit=512;
static char output[4096];
static unsigned used;
DWORD TickGet(void){return 0;}
TCP_SOCKET TCPOpen(DWORD host,BYTE type,WORD port,BYTE purpose)
{assert(!host&&type==TCP_OPEN_SERVER&&port==80&&purpose==TCP_PURPOSE_HTTP);return 0;}
uint8_t TCPWasReset(TCP_SOCKET socket){assert(!socket);uint8_t r=reset;reset=0;return r;}
uint8_t TCPIsConnected(TCP_SOCKET socket){assert(!socket);return connected;}
WORD TCPIsPutReady(TCP_SOCKET socket){assert(!socket);return space;}
WORD TCPPutArray(TCP_SOCKET socket,BYTE *data,WORD length)
{assert(!socket);if(length>limit)length=limit;assert(used+length<sizeof(output));memcpy(output+used,data,length);used+=length;return length;}
void TCPFlush(TCP_SOCKET socket){assert(!socket);}
void TCPDisconnect(TCP_SOCKET socket){assert(!socket);connected=0;}
uint8_t TCPGet(TCP_SOCKET socket,BYTE *byte)
{assert(!socket);if(cursor==strlen(input))return 0;*byte=input[cursor++];return 1;}
uint8_t config_store_busy(void){return 0;}
uint8_t config_store_begin(const config_t *candidate){(void)candidate;return 0;}
static uint8_t asset_ready=1,asset_busy;
uint8_t web_assets_ready(void){return asset_ready;}
uint8_t web_assets_busy(void){return asset_busy;}
uint16_t web_assets_length(void){return 523;}
uint8_t flash_read(uint32_t address,uint8_t *data,uint16_t count)
{assert(address>=WEB_ASSET_BASE+16 && address+count<=WEB_ASSET_BASE+16+523);
 for(unsigned i=0;i<count;++i)data[i]=(uint8_t)((address+i-WEB_ASSET_BASE-16)%251);return 1;}
uint16_t app_format_status(char *out,uint16_t capacity){(void)out;(void)capacity;return 0;}
uint8_t ota_update_busy(void){return 0;}
uint8_t ota_update_chunk_pending(void){return 0;}
uint8_t ota_update_failed(void){return 0;}
void ota_update_abort(void){}
uint16_t ota_update_status(char *p,uint16_t n){(void)p;(void)n;return 0;}
uint16_t ota_update_begin(const uint8_t *p,uint16_t n){(void)p;(void)n;return 503;}
uint16_t ota_update_chunk(const uint8_t *p,uint16_t n){(void)p;(void)n;return 503;}
uint16_t ota_update_commit(void){return 503;}
uint16_t ota_update_rollback(void){return 503;}
int main(int argc,char **argv)
{
 assert(argc==2);int scenario=atoi(argv[1]);
 config_defaults(&device_config);
 if(scenario==3||scenario==4)input="GET / HTTP/1.0\r\n\r\n";
 if(scenario==4)asset_ready=0;
 if(scenario==5){asset_busy=1;input="POST /config HTTP/1.0\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 6\r\n\r\nhost=x";}
 if(scenario==6){input="GET /config HTTP/1.0\r\n\r\n";strcpy(device_config.username,"user\"\\name");strcpy(device_config.password,"hidden-secret");}
 http_server_task(); /* Listen. */
 if(scenario==2)limit=1;
 http_server_task(); /* Parse and queue response. */
 assert(!app_reboot_requested);
 if(scenario==0){space=512;http_server_task();assert(app_reboot_requested&&!connected);}
 if(scenario==1){assert(strstr(output,"{\"rebooting\":true}"));reset=1;connected=0;http_server_task();assert(app_reboot_requested);}
 if(scenario==3){limit=17;space=512;for(unsigned i=0;i<100&&connected;++i)http_server_task();
 assert(!connected && strstr(output,"Content-Encoding: gzip") && strstr(output,"Content-Length: 523"));
 char *body=strstr(output,"\r\n\r\n")+4;assert(used-(unsigned)(body-output)==523);
 for(unsigned i=0;i<523;++i)assert((uint8_t)body[i]==i%251);}
 if(scenario==4){assert(strstr(output,"503"));}
 if(scenario==5){assert(strstr(output,"409"));}
 if(scenario==2){reset=1;connected=0;http_server_task();assert(!app_reboot_requested);}
 if(scenario==6){for(unsigned i=0;i<10&&connected;++i)http_server_task();assert(strstr(output,"\"username\":\"user\\\"\\\\name\""));assert(!strstr(output,"hidden-secret"));assert(strstr(output,"\"password_set\":true"));}
 puts("HTTP reboot: response ACK, immediate peer FIN and incomplete response cases passed");
 return 0;
}
