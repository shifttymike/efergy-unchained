#!/usr/bin/env python3
"""Create bounded binary OTA package and combine permanent boot/application HEX."""
import argparse, json, struct, zlib, hashlib, re
from pathlib import Path
APP_BASE=0x400
BOOT_BASE=0x1cc00
APP_SIZE=BOOT_BASE-APP_BASE
MODEL=0x18676069

def read_hex(path):
    result={};base=0;eof=False
    for line in Path(path).read_text().splitlines():
        if eof:raise ValueError('data after EOF')
        if not line.startswith(':'):raise ValueError('invalid HEX')
        r=bytes.fromhex(line[1:])
        if len(r)!=r[0]+5 or sum(r)&255:raise ValueError('bad HEX checksum')
        n,a,k=r[0],int.from_bytes(r[1:3],'big'),r[3];d=r[4:-1]
        if k==0:
            for i,v in enumerate(d):
                if a+i>65535:raise ValueError('cross-bank HEX')
                key=base+a+i
                if key in result and result[key]!=v:raise ValueError('overlapping HEX')
                result[key]=v
        elif k==4 and n==2:base=int.from_bytes(d,'big')<<16
        elif k==1 and n==0:eof=True
        elif k in (3,5):pass
        else:raise ValueError('unsupported HEX record')
    if not eof:raise ValueError('missing HEX EOF')
    return result

def rec(kind,address,data=b''):
    r=bytes([len(data)])+address.to_bytes(2,'big')+bytes([kind])+data
    return ':'+(r+bytes([-sum(r)&255])).hex().upper()
def write_hex(path,memory):
    lines=[];base=None;addresses=sorted(memory);i=0
    while i<len(addresses):
        a=addresses[i]
        if base!=a>>16:base=a>>16;lines.append(rec(4,0,base.to_bytes(2,'big')))
        data=bytearray([memory[a]]);i+=1
        while i<len(addresses) and addresses[i]==a+len(data) and len(data)<16 and addresses[i]>>16==base:
            data.append(memory[addresses[i]]);i+=1
        lines.append(rec(0,a&65535,bytes(data)))
    Path(path).write_text('\n'.join(lines+[rec(1,0)])+'\n')
def payload(memory):
    if any(a<APP_BASE or (BOOT_BASE<=a<0x1fff8) or a>0x1ffff for a in memory):raise ValueError('application overlaps immutable loader/vectors')
    data=bytes(memory.get(a,255) for a in range(APP_BASE,BOOT_BASE))
    if data[:2]==b'\xff\xff':raise ValueError('missing application entry')
    return data

def header(data,version):
    v=version.encode('ascii')
    if len(data)!=APP_SIZE or not 1<=len(v)<=15:raise ValueError('invalid image/version')
    h=bytearray(b'\xff'*64);h[:8]=b'EFGYOTA1';struct.pack_into('<IIII',h,8,MODEL,APP_BASE,APP_SIZE,zlib.crc32(data));h[24:40]=v.ljust(16,b'\0');struct.pack_into('<I',h,56,zlib.crc32(h[:56]));h[63]=0xa5
    return bytes(h)

def main():
    ap=argparse.ArgumentParser();ap.add_argument('application');ap.add_argument('--version',default=None);ap.add_argument('--bootloader');ap.add_argument('--out',default='build');a=ap.parse_args();out=Path(a.out);out.mkdir(parents=True,exist_ok=True)
    if a.version is None:
        a.version=re.search(r'EFERGY_FIRMWARE_VERSION "([^"\n]+)"',(Path(__file__).resolve().parent.parent/'include/version.h').read_text()).group(1)
    app=read_hex(a.application);data=payload(app);h=header(data,a.version)
    (out/'efergy-update.efu').write_bytes(h+data)
    factory_text=('#include <stdint.h>\n#define FACTORY_APP_CRC 0x%08xul\nstatic const uint8_t factory_header[64]={%s};\n'%(zlib.crc32(data),','.join(str(x) for x in h)))
    factory_path=out/'factory_image.h'
    if not factory_path.exists() or factory_path.read_text()!=factory_text:factory_path.write_text(factory_text)
    m={'version':a.version,'model':hex(MODEL),'application_base':hex(APP_BASE),'bootloader_base':hex(BOOT_BASE),'application_bytes':APP_SIZE,'payload_crc32':'%08x'%zlib.crc32(data),'package_sha256':hashlib.sha256(h+data).hexdigest(),'hardware_validated':False,'oem_transition_validated':False}
    if a.bootloader:
        boot=read_hex(a.bootloader)
        if any(0x400<=x<BOOT_BASE or x>0x1ffff for x in boot):raise ValueError('boot image overlaps application')
        # Only the immutable loader supplies configuration words.
        merged={x:v for x,v in app.items() if APP_BASE<=x<BOOT_BASE}
        for x,v in boot.items():
            if x in merged:raise ValueError('image overlap')
            merged[x]=v
        write_hex(out/'efergy-factory.hex',merged)
        m['factory_sha256']=hashlib.sha256((out/'efergy-factory.hex').read_bytes()).hexdigest()
    (out/'firmware-package.json').write_text(json.dumps(m,indent=2)+'\n')
if __name__=='__main__':main()
