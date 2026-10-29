#include <string.h>
#include "http_parser.h"
static uint8_t lower(uint8_t c) {return c>='A'&&c<='Z'?(uint8_t)(c+32):c;}
static uint8_t prefix(const char *s,const char *key)
{
    while(*key) if(lower((uint8_t)*s++)!=lower((uint8_t)*key++)) return 0;
    return 1;
}
static uint8_t decimal(const char *s,uint16_t *value)
{
    uint32_t n=0;
    if(!*s) return 0;
    while(*s) {if(*s<'0'||*s>'9')return 0;n=n*10u+(uint8_t)(*s++-'0');if(n>65535u)return 0;}
    *value=(uint16_t)n;return 1;
}
void http_parser_init(http_parser_t *p) {memset(p,0,sizeof(*p));}
static uint16_t line(http_parser_t *p)
{
    char *s=p->buffer,*space,*value;
    uint16_t n;
    if(!p->state) {
        if(prefix(s,"GET ")) {p->post=0;s+=4;}
        else if(prefix(s,"POST ")) {p->post=1;s+=5;}
        else return 405;
        space=strchr(s,' ');if(!space)return 400;*space++=0;
        if(strcmp(space,"HTTP/1.0")&&strcmp(space,"HTTP/1.1"))return 400;
        if(!strcmp(s,"/status"))p->route=HTTP_STATUS;
        else if(!strcmp(s,"/"))p->route=HTTP_INDEX;
        else if(!strcmp(s,"/config"))p->route=HTTP_CONFIG;
        else if(!strcmp(s,"/flash.bin"))p->route=HTTP_FLASH;
        else if(!strcmp(s,"/update"))p->route=HTTP_UPDATE;
        else if(!strcmp(s,"/update/data"))p->route=HTTP_UPDATE_DATA;
        else if(!strcmp(s,"/update/commit"))p->route=HTTP_UPDATE_COMMIT;
        else if(!strcmp(s,"/update/rollback"))p->route=HTTP_UPDATE_ROLLBACK;
        else if(!strcmp(s,"/update/cancel"))p->route=HTTP_UPDATE_CANCEL;
        else if(!strcmp(s,"/reboot"))p->route=HTTP_REBOOT;
#ifdef OEM_RESTORE_BUILD
        else if(!strcmp(s,"/restore/oem"))p->route=HTTP_OEM;
        else if(!strcmp(s,"/restore/oem/data"))p->route=HTTP_OEM_DATA;
        else if(!strcmp(s,"/restore/oem/commit"))p->route=HTTP_OEM_COMMIT;
#endif
        else return 404;
        if((p->post && p->route!=HTTP_CONFIG && p->route!=HTTP_REBOOT && p->route<HTTP_UPDATE)||
           (!p->post && (p->route==HTTP_REBOOT||(p->route>HTTP_UPDATE
#ifdef OEM_RESTORE_BUILD
            &&p->route!=HTTP_OEM
#endif
           ))))return 405;
        p->state=1;return 0;
    }
    if(!*s) {
        if(p->post && !p->has_length)return 411;
        if(!p->post && p->content_length)return 400;
        if((p->route==HTTP_REBOOT||(p->route>=HTTP_UPDATE_COMMIT
#ifdef OEM_RESTORE_BUILD
            &&p->route!=HTTP_OEM&&p->route!=HTTP_OEM_DATA
#endif
           )) && p->content_length)return 400;
        if(p->post && p->route==HTTP_CONFIG && !p->form_type)return 415;
        if(p->post&&(p->route==HTTP_UPDATE||p->route==HTTP_UPDATE_DATA)&&p->form_type!=2)return 415;
#ifdef OEM_RESTORE_BUILD
        if(p->post&&(p->route==HTTP_OEM||p->route==HTTP_OEM_DATA)&&p->form_type!=2)return 415;
#endif
        p->state=p->content_length?2:3;
        return p->state==3?200:0;
    }
    value=strchr(s,':');if(!value)return 400;
    *value++=0;while(*value==' '||*value=='\t')++value;
    if(!strcmp(s,"Content-Length")||prefix(s,"content-length")) {
        if(strlen(s)!=14 || p->has_length || !decimal(value,&n))return 400;
        if(n>=HTTP_BUFFER_SIZE)return 413;
        p->content_length=n;p->has_length=1;
    } else if(prefix(s,"transfer-encoding"))return 400;
    else if(prefix(s,"expect"))return 417;
    else if(prefix(s,"content-type")) {
        if(p->form_type)return 400;
        if(p->route>=HTTP_UPDATE){if(strcmp(value,"application/octet-stream"))return 415;p->form_type=2;return 0;}
        if(!prefix(value,"application/x-www-form-urlencoded") ||
           (value[33] && value[33]!=';'))return 415;
        p->form_type=1;
    }
    return 0;
}
uint16_t http_parser_feed(http_parser_t *p,uint8_t byte)
{
    uint16_t result;
    if(p->state>=3)return 400;
    if(p->state==2) {
        if(!byte&&p->route<HTTP_UPDATE)return 400;
        p->buffer[p->used++]=(char)byte;
        if(p->used==p->content_length) {p->buffer[p->used]=0;p->state=3;return 200;}
        return 0;
    }
    if(++p->header_bytes>2048)return 431;
    if(byte=='\r') {if(p->saw_cr)return 400;p->saw_cr=1;return 0;}
    if(byte=='\n') {
        if(!p->saw_cr)return 400;
        p->buffer[p->used]=0;result=line(p);p->used=0;p->saw_cr=0;
        if(result==200)p->buffer[0]=0;
        return result;
    }
    if(p->saw_cr || byte<32 || byte==127)return 400;
    if(p->used>=511)return 431;
    p->buffer[p->used++]=(char)byte;return 0;
}
static int8_t hex(uint8_t c)
{
    if(c>='0'&&c<='9')return (int8_t)(c-'0');c=lower(c);
    return c>='a'&&c<='f'?(int8_t)(c-'a'+10):-1;
}
static uint8_t decode(char *s)
{
    char *out=s;int8_t a,b;uint8_t c;
    while(*s) {
        c=(uint8_t)*s++;
        if(c=='%') {
            if(!s[0]||!s[1]||(a=hex((uint8_t)s[0]))<0||(b=hex((uint8_t)s[1]))<0)return 0;
            c=(uint8_t)((a<<4)|b);s+=2;
        }else if(c=='+')c=' ';
        if(c<32||c>126)return 0;
        *out++=(char)c;
    }
    *out=0;return 1;
}
static uint8_t ipv4(char *s,uint8_t out[4])
{
    uint8_t i;uint16_t n;char *dot;
    for(i=0;i<4;++i) {
        dot=strchr(s,'.');if(i<3 && !dot)return 0;if(i==3 && dot)return 0;
        if(dot)*dot=0;if(!decimal(s,&n)||n>255)return 0;out[i]=(uint8_t)n;
        if(dot)s=dot+1;
    }
    return 1;
}
uint8_t config_form_apply(char *body,config_t *c)
{
    char *next,*value;uint16_t seen=0,flag,n;uint8_t clear=0;
    if(!*body)return 0;
    do {
        next=strchr(body,'&');if(next)*next++=0;
        value=strchr(body,'=');if(!value)return 0;*value++=0;
        if(!decode(body)||!decode(value))return 0;
        if(!strcmp(body,"host"))flag=1;
        else if(!strcmp(body,"port"))flag=2;
        else if(!strcmp(body,"username"))flag=4;
        else if(!strcmp(body,"password"))flag=8;
        else if(!strcmp(body,"clear_password"))flag=16;
        else if(!strcmp(body,"dhcp"))flag=32;
        else if(!strcmp(body,"ip"))flag=64;
        else if(!strcmp(body,"mask"))flag=128;
        else if(!strcmp(body,"gateway"))flag=256;
        else if(!strcmp(body,"dns"))flag=512;
        else if(!strcmp(body,"voltage_v"))flag=1024;
        else if(!strcmp(body,"device_name"))flag=2048;
        else return 0;
        if(seen&flag)return 0;seen|=flag;
        switch(flag) {
        case 1:if(strlen(value)>=sizeof(c->host))return 0;memset(c->host,0,sizeof(c->host));strcpy(c->host,value);break;
        case 2:if(!decimal(value,&n)||!n)return 0;c->port=n;break;
        case 4:if(strlen(value)>=sizeof(c->username))return 0;memset(c->username,0,sizeof(c->username));strcpy(c->username,value);break;
        case 8:if(strlen(value)>=sizeof(c->password))return 0;if(*value){memset(c->password,0,sizeof(c->password));strcpy(c->password,value);}break;
        case 16:if(strcmp(value,"1"))return 0;clear=1;break;
        case 32:if(strcmp(value,"0")&&strcmp(value,"1"))return 0;c->dhcp=(uint8_t)(*value-'0');break;
        case 64:if(!ipv4(value,c->ip))return 0;break;
        case 128:if(!ipv4(value,c->mask))return 0;break;
        case 256:if(!ipv4(value,c->gateway))return 0;break;
        case 512:if(!ipv4(value,c->dns))return 0;break;
        case 1024:if(!decimal(value,&n)||!n||n>CONFIG_VOLTAGE_MAX)return 0;c->voltage_v=n;break;
        case 2048:if(strlen(value)>=sizeof(c->device_name))return 0;memset(c->device_name,0,sizeof(c->device_name));strcpy(c->device_name,value);break;
        default:return 0;
        }
        body=next;
    }while(body && *body);
    if(clear)memset(c->password,0,sizeof(c->password));
    return config_valid(c);
}
