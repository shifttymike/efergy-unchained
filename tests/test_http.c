#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "http_parser.h"
static uint16_t parse(http_parser_t *p,const char *s)
{
    uint16_t result=0;http_parser_init(p);
    while(*s && !result)result=http_parser_feed(p,(uint8_t)*s++);
    return result;
}
int main(void)
{
    http_parser_t p;config_t c;char text[1200];unsigned i;
    assert(parse(&p,"GET /status HTTP/1.1\r\nHost: test\r\n\r\n")==200 && p.route==HTTP_STATUS);
    assert(parse(&p,"POST /config HTTP/1.1\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 6\r\n\r\nport=1")==200);
    assert(!strcmp(p.buffer,"port=1"));config_defaults(&c);assert(config_form_apply(p.buffer,&c)&&c.port==1);
    assert(parse(&p,"POST /config HTTP/1.1\r\nContent-Length: 0\r\nContent-Length: 0\r\n\r\n")==400);
    assert(parse(&p,"POST /config HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n")==400);
    assert(parse(&p,"POST /config HTTP/1.1\r\nContent-Length: 768\r\n\r\n")==413);
    assert(parse(&p,"POST /config HTTP/1.1\r\n\r\n")==411);
    assert(parse(&p,"GET /status HTTP/1.1\n\n")==400);
    assert(parse(&p,"PUT /config HTTP/1.1\r\n\r\n")==405);
    assert(parse(&p,"GET /missing HTTP/1.1\r\n\r\n")==404);
    /* Browser navigation Accept/User-Agent headers exceed the old 127 limit. */
    strcpy(text,"GET / HTTP/1.1\r\nUser-Agent: ");size_t header_start=strlen(text);memset(text+header_start,'a',300);
    text[header_start+300]=0;strcat(text,"\r\n\r\n");assert(parse(&p,text)==200);
    memset(text,'a',sizeof(text));memcpy(text,"GET /status HTTP/1.1\r\nX:",23);text[sizeof(text)-1]=0;
    assert(parse(&p,text)==431);
    for(i=0;i<6;++i){http_parser_init(&p);p.state=2;p.content_length=6;
        for(unsigned j=0;j<i;++j)assert(http_parser_feed(&p,(uint8_t)"host=x"[j])==0);
        for(unsigned j=i;j<6;++j)assert(http_parser_feed(&p,(uint8_t)"host=x"[j])==(j==5?200:0));
        assert(!strcmp(p.buffer,"host=x"));}
    assert(parse(&p,"GET /update HTTP/1.1\r\n\r\n")==200&&p.route==HTTP_UPDATE);
    assert(parse(&p,"GET /update/data HTTP/1.1\r\n\r\n")==405);
    assert(parse(&p,"POST /update/data HTTP/1.1\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 5\r\n\r\n")==415);
    assert(parse(&p,"POST /update/data HTTP/1.1\r\nContent-Type: application/octet-stream\r\nContent-Length: 5\r\n\r\n")==0);
    for(unsigned j=0;j<5;++j)assert(http_parser_feed(&p,0)==(j==4?200:0));
    assert(p.content_length==5&&!memcmp(p.buffer,"\0\0\0\0\0",5));
    assert(parse(&p,"POST /update/commit HTTP/1.1\r\nContent-Length: 1\r\n\r\nx")==400);
    config_defaults(&c);strcpy(c.password,"old");
    strcpy(text,"host=broker.local&port=1883&username=a%2Bb&password=");
    assert(config_form_apply(text,&c));assert(!strcmp(c.username,"a+b")&&!strcmp(c.password,"old"));
    strcpy(text,"password=hello+world%26%3D&clear_password=1");assert(config_form_apply(text,&c)&&!c.password[0]);
    strcpy(text,"host=x&host=y");assert(!config_form_apply(text,&c));
    strcpy(text,"password=%00evil");assert(!config_form_apply(text,&c));
    strcpy(text,"password=%xz");assert(!config_form_apply(text,&c));
    strcpy(text,"port=65536");assert(!config_form_apply(text,&c));
    strcpy(text,"ip=192.168.23.8&mask=255.255.255.0&dhcp=0");assert(config_form_apply(text,&c)&&c.ip[3]==8&&!c.dhcp);
    strcpy(text,"ip=192.168.23.999");assert(!config_form_apply(text,&c));
    strcpy(text,"voltage_v=240");assert(config_form_apply(text,&c)&&c.voltage_v==240);
    strcpy(text,"voltage_v=0");assert(!config_form_apply(text,&c));
    strcpy(text,"voltage_v=501");assert(!config_form_apply(text,&c));
    strcpy(text,"voltage_v=230.5");assert(!config_form_apply(text,&c));
    strcpy(text,"voltage_v=230&voltage_v=240");assert(!config_form_apply(text,&c));
    strcpy(text,"voltage_v=1");assert(config_form_apply(text,&c)&&c.voltage_v==1);
    strcpy(text,"voltage_v=500");assert(config_form_apply(text,&c)&&c.voltage_v==500);
    assert(parse(&p,"GET / HTTP/1.1\r\nHost: test\r\n\r\n")==200 && p.route==HTTP_INDEX);
    strcpy(text,"device_name=Kitchen+meter");assert(config_form_apply(text,&c)&&!strcmp(c.device_name,"Kitchen meter"));
    strcpy(text,"device_name=");assert(!config_form_apply(text,&c));
    strcpy(text,"device_name=+++");assert(!config_form_apply(text,&c));
    strcpy(text,"device_name=01234567890123456789012345678901");assert(!config_form_apply(text,&c));
    strcpy(text,"device_name=Kitchen%22meter%22");assert(config_form_apply(text,&c)&&!strcmp(c.device_name,"Kitchen\"meter\""));
    puts("HTTP: fragmented input, framing/size rejection, bounded config patches and credential semantics passed");
#ifdef OEM_RESTORE_BUILD
    assert(parse(&p,"GET /restore/oem HTTP/1.1\r\n\r\n")==200&&p.route==HTTP_OEM);
    assert(parse(&p,"GET /restore/oem/data HTTP/1.1\r\n\r\n")==405);
    assert(parse(&p,"POST /restore/oem HTTP/1.1\r\nContent-Type: application/octet-stream\r\nContent-Length: 64\r\n\r\n")==0);
    for(unsigned j=0;j<64;++j)assert(http_parser_feed(&p,0)==(j==63?200:0));
    assert(parse(&p,"POST /restore/oem/data HTTP/1.1\r\nContent-Type: application/octet-stream\r\nContent-Length: 260\r\n\r\n")==0);
    for(unsigned j=0;j<260;++j)assert(http_parser_feed(&p,0)==(j==259?200:0));
    assert(parse(&p,"POST /restore/oem/commit HTTP/1.1\r\nContent-Length: 0\r\n\r\n")==200);
    assert(parse(&p,"POST /restore/oem/commit HTTP/1.1\r\nContent-Length: 1\r\n\r\nx")==400);
    puts("OEM restore binary endpoints, maximum chunk and empty commit framing passed");
#endif
}
