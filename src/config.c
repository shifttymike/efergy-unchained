#include <string.h>
#include "config.h"

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static void put32(uint8_t *p, uint32_t v)
{
    uint8_t i;
    for(i=0;i<4;++i) {p[i]=(uint8_t)v;v>>=8;}
}
static uint32_t crc_record(const uint8_t *record,uint16_t size)
{
    uint32_t crc=0xfffffffful;
    uint16_t i;
    uint8_t j;
    for(i=0;i<size;++i) {
        if(i>=12 && i<=16) continue;
        crc^=record[i];
        for(j=0;j<8;++j) crc=(crc>>1)^((crc&1u)?0xedb88320ul:0ul);
    }
    return crc^0xfffffffful;
}
static uint8_t terminated(const char *s,uint8_t capacity)
{
    uint8_t i;
    for(i=0;i<capacity;++i) {
        if(!s[i]) return 1;
        if((uint8_t)s[i]<32u || (uint8_t)s[i]>126u) return 0;
    }
    return 0;
}
static uint8_t unicast(const uint8_t *ip)
{
    return ip[0]!=0 && ip[0]!=127 && ip[0]<224;
}
void config_defaults(config_t *c)
{
    static const uint8_t ip[]={192,168,123,1},mask[]={255,255,255,0},gw[]={192,168,123,254};
    memset(c,0,sizeof(*c));
    c->port=1883;c->dhcp=1;c->radio_profile=1;
    c->voltage_v=CONFIG_VOLTAGE_DEFAULT;
    strcpy(c->device_name,"Efergy gateway");
    memcpy(c->ip,ip,4);memcpy(c->mask,mask,4);memcpy(c->gateway,gw,4);memcpy(c->dns,gw,4);
}
uint8_t config_valid(const config_t *c)
{
    uint8_t i, host_bits=0, host_nonzero=0, host_not_ones=0;
    uint32_t mask, inverse;
    if(!terminated(c->host,CONFIG_HOST_SIZE) || !terminated(c->username,CONFIG_USER_SIZE) ||
       !terminated(c->password,CONFIG_PASSWORD_SIZE) || !c->port || c->dhcp>1 || c->radio_profile>1 ||
       !c->voltage_v || c->voltage_v>CONFIG_VOLTAGE_MAX ||
       !terminated(c->device_name,CONFIG_NAME_SIZE)) return 0;
    for(i=0;c->device_name[i]==' ';++i) {}
    if(!c->device_name[i])return 0;
    for(i=0;c->host[i];++i) {
        uint8_t b=(uint8_t)c->host[i];
        if(!((b>='a'&&b<='z') || (b>='A'&&b<='Z') || (b>='0'&&b<='9') || b=='.' || b=='-')) return 0;
    }
    if(!unicast(c->ip)) return 0;
    mask=((uint32_t)c->mask[0]<<24)|((uint32_t)c->mask[1]<<16)|((uint32_t)c->mask[2]<<8)|c->mask[3];
    inverse=~mask;
    if(!mask || !inverse || (inverse&(inverse+1u))) return 0;
    for(i=0;i<4;++i) {
        host_bits=(uint8_t)~c->mask[i];
        host_nonzero|=c->ip[i]&host_bits;
        host_not_ones|=(uint8_t)~c->ip[i]&host_bits;
    }
    if(!host_nonzero || !host_not_ones) return 0;
    if(get32(c->gateway) && !unicast(c->gateway)) return 0;
    if(get32(c->dns) && !unicast(c->dns)) return 0;
    return 1;
}
uint8_t config_encode(uint8_t *r,const config_t *c,uint32_t sequence,uint8_t committed)
{
    uint8_t *p;
    if(!config_valid(c)) return 0;
    memset(r,0,CONFIG_RECORD_SIZE);
    memcpy(r,"EFG2",4);r[4]=3;r[5]=214;
    put32(r+8,sequence);r[16]=committed?0xa5:0xff;
    p=r+17;
    memcpy(p,c->host,CONFIG_HOST_SIZE);p+=CONFIG_HOST_SIZE;
    memcpy(p,c->username,CONFIG_USER_SIZE);p+=CONFIG_USER_SIZE;
    memcpy(p,c->password,CONFIG_PASSWORD_SIZE);p+=CONFIG_PASSWORD_SIZE;
    *p++=(uint8_t)c->port;*p++=(uint8_t)(c->port>>8);
    *p++=c->dhcp;*p++=c->radio_profile;
    memcpy(p,c->ip,4);memcpy(p+4,c->mask,4);memcpy(p+8,c->gateway,4);memcpy(p+12,c->dns,4);
    r[197]=(uint8_t)c->voltage_v;r[198]=(uint8_t)(c->voltage_v>>8);
    memcpy(r+199,c->device_name,CONFIG_NAME_SIZE);
    put32(r+12,crc_record(r,CONFIG_RECORD_SIZE));
    return 1;
}
uint8_t config_decode(const uint8_t *r,config_t *c,uint32_t *sequence)
{
    const uint8_t *p=r+17;
    uint16_t size;
    if(r[4]==1 && r[5]==180)size=CONFIG_RECORD_V1_SIZE;
    else if(r[4]==2 && r[5]==182)size=CONFIG_RECORD_V2_SIZE;
    else if(r[4]==3 && r[5]==214)size=CONFIG_RECORD_SIZE;
    else return 0;
    if(memcmp(r,"EFG2",4) || r[6] || r[7] || r[16]!=0xa5 ||
       get32(r+12)!=crc_record(r,size)) return 0;
    memset(c,0,sizeof(*c));
    memcpy(c->host,p,CONFIG_HOST_SIZE);p+=CONFIG_HOST_SIZE;
    memcpy(c->username,p,CONFIG_USER_SIZE);p+=CONFIG_USER_SIZE;
    memcpy(c->password,p,CONFIG_PASSWORD_SIZE);p+=CONFIG_PASSWORD_SIZE;
    c->port=(uint16_t)(p[0]|((uint16_t)p[1]<<8));p+=2;
    c->dhcp=*p++;c->radio_profile=*p++;
    memcpy(c->ip,p,4);memcpy(c->mask,p+4,4);memcpy(c->gateway,p+8,4);memcpy(c->dns,p+12,4);
    c->voltage_v=size==CONFIG_RECORD_V1_SIZE?CONFIG_VOLTAGE_DEFAULT:
        (uint16_t)(r[197]|((uint16_t)r[198]<<8));
    if(size==CONFIG_RECORD_SIZE)memcpy(c->device_name,r+199,CONFIG_NAME_SIZE);
    else strcpy(c->device_name,"Efergy gateway");
    if(!config_valid(c)) return 0;
    *sequence=get32(r+8);
    return 1;
}
uint8_t config_sequence_newer(uint32_t a,uint32_t b)
{
    return a!=b && (uint32_t)(a-b)<0x80000000ul;
}
uint8_t config_text_json(char *out,uint8_t capacity,const char *text)
{
    uint8_t i,n=0;
    for(i=0;text[i];++i) {
        if(text[i]=='"'||text[i]=='\\') {
            if(n+2u>=capacity)return 0;
            out[n++]='\\';
        }else if(n+1u>=capacity)return 0;
        out[n++]=text[i];
    }
    if(n>=capacity)return 0;
    out[n]=0;return n;
}

uint8_t config_name_json(char *out,uint8_t capacity,const config_t *c)
{ return config_text_json(out,capacity,c->device_name); }
