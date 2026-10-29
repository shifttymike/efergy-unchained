#ifndef TEST_LEGACY_CONFIG_H
#define TEST_LEGACY_CONFIG_H
/* Construct the deployed v1 wire format, with an independent CRC calculation.
 * The old format ends at byte 196 and has no voltage field. */
static void make_legacy(uint8_t *r,const config_t *config,uint32_t sequence,unsigned version)
{
    uint32_t crc=0xffffffffu;
    assert(config_encode(r,config,sequence,1));
    unsigned size=version==1?CONFIG_RECORD_V1_SIZE:CONFIG_RECORD_V2_SIZE;
    r[4]=(uint8_t)version;r[5]=version==1?180:182;
    memset(r+size,255,CONFIG_RECORD_SIZE-size);
    for(unsigned i=0;i<size;++i) {
        if(i>=12 && i<=16)continue;
        crc^=r[i];
        for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1u)?0xedb88320u:0u);
    }
    crc^=0xffffffffu;
    for(unsigned i=0;i<4;++i){r[12+i]=(uint8_t)crc;crc>>=8;}
}
#define make_v1(r,c,s) make_legacy(r,c,s,1)
#define make_v2(r,c,s) make_legacy(r,c,s,2)
#endif
