#include "TCPIP Stack/TCPIP.h"
#include "app.h"
#include "board.h"
#include "config_store.h"
#include "http_parser.h"
#include "web_assets.h"
#include "ota_update.h"
#include "oem_restore.h"
#ifndef EFERGY_FLASH_DUMP
#define EFERGY_FLASH_DUMP 0
#endif
static TCP_SOCKET socket=INVALID_SOCKET;
static http_parser_t request;
static uint8_t state,reboot_after;
#ifdef OEM_RESTORE_BUILD
static uint8_t restore_chunk;
#endif
static uint16_t length,sent;
static uint32_t flash_position,flash_end;
static DWORD started;
/* Response body is composed above a reserved header; no second TX buffer. */
static void response(uint16_t code,uint16_t body_length)
{
    uint16_t n;
    if(body_length>=HTTP_BUFFER_SIZE-128u) {
        code=500;strcpy(request.buffer+128,"{\"error\":500}");body_length=13;
    }
    n=(uint16_t)snprintf(request.buffer,128,
        "HTTP/1.0 %u %s\r\nContent-Type: application/json\r\nContent-Length: %u\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n",
        code,code==200?"OK":code==202?"Accepted":"Error",body_length);
    if(n>=128){TCPDisconnect(socket);state=0;return;}
    memmove(request.buffer+n,request.buffer+128,body_length);
    length=n+body_length;sent=0;state=2;
}
static void error(uint16_t code)
{
    uint16_t n=(uint16_t)snprintf(request.buffer+128,HTTP_BUFFER_SIZE-128,"{\"error\":%u}",code);
    response(code,n);
}
static uint16_t config_json(char *out,uint16_t capacity)
{
    char *name=(char *)&config_scratch;
    char *user=name+CONFIG_NAME_SIZE*2u;
    if(!config_name_json(name,CONFIG_NAME_SIZE*2u,&device_config))return capacity;
    config_text_json(user,CONFIG_USER_SIZE*2u,device_config.username);
    /* Host is validated; name/username are escaped; password is never returned. */
    return (uint16_t)snprintf(out,capacity,
        "{\"host\":\"%s\",\"port\":%u,\"username\":\"%s\",\"username_set\":%s,\"password_set\":%s,"
        "\"dhcp\":%u,\"ip\":\"%u.%u.%u.%u\",\"mask\":\"%u.%u.%u.%u\","
        "\"gateway\":\"%u.%u.%u.%u\",\"dns\":\"%u.%u.%u.%u\","
        "\"sequence\":%lu,\"save_state\":%u,\"save_error\":%u,\"writes_enabled\":%u,\"voltage_v\":%u,\"device_name\":\"%s\"}",
        device_config.host,device_config.port,user,device_config.username[0]?"true":"false",device_config.password[0]?"true":"false",
        device_config.dhcp,device_config.ip[0],device_config.ip[1],device_config.ip[2],device_config.ip[3],
        device_config.mask[0],device_config.mask[1],device_config.mask[2],device_config.mask[3],
        device_config.gateway[0],device_config.gateway[1],device_config.gateway[2],device_config.gateway[3],
        device_config.dns[0],device_config.dns[1],device_config.dns[2],device_config.dns[3],
        (unsigned long)config_sequence,config_store_state,config_store_error,CONFIG_WRITES_ENABLED,device_config.voltage_v,name);
}
static void dispatch(void)
{
    uint16_t n;config_t *candidate=&config_scratch;
    reboot_after=0;
#ifdef OEM_RESTORE_BUILD
    if(request.route>=HTTP_OEM){
        if(!request.post){response(200,oem_restore_status(request.buffer+128,HTTP_BUFFER_SIZE-128));return;}
        if(request.route==HTTP_OEM)n=oem_restore_begin((uint8_t *)request.buffer,request.content_length);
        else if(request.route==HTTP_OEM_DATA){n=oem_restore_chunk((uint8_t *)request.buffer,request.content_length);if(n==202){restore_chunk=1;state=5;return;}}
        else n=oem_restore_commit();
        if(n!=202){error(n);return;}
        strcpy(request.buffer+128,"{\"accepted\":true}");response(202,17);return;
    }
#endif
    if(request.route>=HTTP_UPDATE){
        if(request.post&&oem_restore_busy()){error(409);return;}
        if(!request.post){response(200,ota_update_status(request.buffer+128,HTTP_BUFFER_SIZE-128));return;}
        if(request.route==HTTP_UPDATE)n=ota_update_begin((uint8_t *)request.buffer,request.content_length);
        else if(request.route==HTTP_UPDATE_DATA){n=ota_update_chunk((uint8_t *)request.buffer,request.content_length);if(n==202){
#ifdef OEM_RESTORE_BUILD
            restore_chunk=0;
#endif
            state=5;return;}}
        else if(request.route==HTTP_UPDATE_COMMIT)n=ota_update_commit();
        else if(request.route==HTTP_UPDATE_ROLLBACK)n=ota_update_rollback();
        else {ota_update_abort();n=202;}
        if(n!=202){error(n);return;}
        strcpy(request.buffer+128,"{\"accepted\":true}");response(202,17);return;
    }
    if(request.route==HTTP_INDEX) {
        if(!web_assets_ready()){error(503);return;}
        length=(uint16_t)snprintf(request.buffer,sizeof(request.buffer),
            "HTTP/1.0 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Encoding: gzip\r\nContent-Length: %u\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n",web_assets_length());
        flash_position=WEB_ASSET_BASE+16ul;flash_end=flash_position+web_assets_length();
        sent=0;state=3;return;
    }
    if(request.route==HTTP_FLASH) {
#if EFERGY_FLASH_DUMP
        if(config_store_busy()){error(409);return;}
        if(flash_id()!=0xbf258dul){error(503);return;}
        length=(uint16_t)snprintf(request.buffer,sizeof(request.buffer),
            "HTTP/1.0 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: 524288\r\nConnection: close\r\n\r\n");
        flash_position=0;flash_end=0x80000ul;sent=0;state=3;return;
#else
        error(404);return;
#endif
    }
    if(request.route==HTTP_CONFIG && request.post) {
        if(config_store_busy() || web_assets_busy() || ota_update_busy() || oem_restore_busy()){error(409);return;}
        memcpy(candidate,&device_config,sizeof(*candidate));
        if(!config_form_apply(request.buffer,candidate)){memset(candidate,0,sizeof(*candidate));error(400);return;}
        if(!memcmp(candidate,&device_config,sizeof(*candidate))) {
            memset(candidate,0,sizeof(*candidate));strcpy(request.buffer+128,"{\"unchanged\":true}");response(200,18);return;
        }
        if(!config_store_begin(candidate)){memset(candidate,0,sizeof(*candidate));error(503);return;}
        memset(candidate,0,sizeof(*candidate));
        n=(uint16_t)snprintf(request.buffer+128,HTTP_BUFFER_SIZE-128,"{\"saving\":true,\"next_sequence\":%lu}",(unsigned long)(config_sequence+1u));
        response(202,n);return;
    }
    if(request.route==HTTP_REBOOT) {
        if(config_store_busy() || web_assets_busy() || ota_update_busy() || oem_restore_busy()){error(409);return;}
        strcpy(request.buffer+128,"{\"rebooting\":true}");response(200,18);reboot_after=1;return;
    }
    if(request.route==HTTP_CONFIG)n=config_json(request.buffer+128,HTTP_BUFFER_SIZE-128);
    else n=app_format_status(request.buffer+128,HTTP_BUFFER_SIZE-128);
    response(200,n);
}
void http_server_task(void)
{
    uint8_t byte;uint16_t result;
    if(socket==INVALID_SOCKET){socket=TCPOpen(0,TCP_OPEN_SERVER,80,TCP_PURPOSE_HTTP);return;}
    if(TCPWasReset(socket)) {
#ifdef OEM_RESTORE_BUILD
        if(state==5&&restore_chunk)oem_restore_abort();
#endif
        if(state==5)ota_update_abort();
        /* A client may FIN immediately after reading the complete response. */
        if(reboot_after && state==4){app_reboot_requested=1;return;}
        state=0;reboot_after=0;
    }
    if(!TCPIsConnected(socket))return;
    if(!state){http_parser_init(&request);started=TickGet();state=1;}
    if((DWORD)(TickGet()-started)>(DWORD)((state==3?120ul:state==5?30ul:10ul)*TICK_SECOND)) {
#ifdef OEM_RESTORE_BUILD
        if(state==5&&restore_chunk)oem_restore_abort();
#endif
        if(state==5)ota_update_abort();TCPDisconnect(socket);state=0;return;
    }
    if(state==1) {
        while(TCPGet(socket,&byte)) {
            result=http_parser_feed(&request,byte);
            if(result){if(result==200)dispatch();else error(result);break;}
        }
    }
#ifdef OEM_RESTORE_BUILD
    if(state==5&&restore_chunk){if(!oem_restore_chunk_pending()){if(oem_restore_failed())error(500);else{strcpy(request.buffer+128,"{\"accepted\":true}");response(200,17);}}}
    else
#endif
    if(state==5&&!ota_update_chunk_pending()){if(ota_update_failed())error(500);else{strcpy(request.buffer+128,"{\"accepted\":true}");response(200,17);}}
    if(state==2 || state==3) {
        sent+=TCPPutArray(socket,(BYTE *)request.buffer+sent,length-sent);TCPFlush(socket);
        if(sent==length) {
            if(state==3 && flash_position<flash_end) {
                if(TCPIsPutReady(socket)<256)return;
                length=(uint16_t)((flash_end-flash_position)>256ul?256ul:(flash_end-flash_position));
                if(!flash_read(flash_position,(uint8_t *)request.buffer,length)){TCPDisconnect(socket);state=0;return;}
                flash_position+=length;sent=0;
            }else state=4;
        }
    }
    if(state==4 && TCPIsPutReady(socket)>=512) {
        TCPDisconnect(socket);state=0;if(reboot_after)app_reboot_requested=1;
    }
}
