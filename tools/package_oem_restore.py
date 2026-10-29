#!/usr/bin/env python3
"""Prepare a checked stock image and a one-off custom-to-OEM restore bundle."""
import argparse,hashlib,json,struct,zipfile,zlib
from pathlib import Path
from package_firmware import read_hex,write_hex,MODEL

def stock_images(path):
    m=read_hex(path)
    if any(a>=0x20000 for a in m):raise ValueError('OEM image outside PIC memory')
    raw=bytes(m.get(a,255) for a in range(0x20000))
    if raw[:4]!=bytes.fromhex('00efe6f0'):raise ValueError('unrecognized OEM loader vector')
    if raw[0x1b131:0x1b137]!=b'3.7.1\0' or b'EH433M3v1' not in raw:
        raise ValueError('only the inspected EH433M3v1 / OEM 3.7.1 image is supported')
    entry=raw[0x1cbc0:0x1cbc4]
    if entry==raw[:4] or entry==b'\xff'*4 or entry[1]!=0xef:raise ValueError('bad OEM application stub')
    app={a+i:raw[a+i] for a in list(range(0,0x1cc00,16))+list(range(0x1fc00,0x20000,16)) if any(v!=255 for v in raw[a:a+16]) for i in range(16)}
    for i,v in enumerate(entry):app[i]=v
    return raw,app

def prepare(path,out):
    raw,app=stock_images(path);out=Path(out);out.mkdir(parents=True,exist_ok=True)
    write_hex(out/'oem-application.hex',app);hexf=(out/'oem-application.hex').read_bytes()
    if len(hexf)+4096>0x4f000:raise ValueError('OEM staging exceeds allowed flash range')
    h=bytearray(64);h[:8]=b'EFGYOEM1';struct.pack_into('<IIIII',h,8,MODEL,len(raw),zlib.crc32(raw),len(hexf),zlib.crc32(hexf));h[28:60]=hashlib.md5(hexf).hexdigest().encode();struct.pack_into('<I',h,60,zlib.crc32(h[:60]))
    (out/'oem-pic.bin').write_bytes(raw);(out/'restore-header.bin').write_bytes(h)
    (out/'oem_image.h').write_text('#define OEM_RAW_CRC 0x%08xul\n#define OEM_HEX_CRC 0x%08xul\n#define OEM_HEX_LENGTH %dul\n#define OEM_HEX_MD5 "%s"\nstatic const uint8_t oem_restore_header[64]={%s};\n'%(zlib.crc32(raw),zlib.crc32(hexf),len(hexf),hashlib.md5(hexf).hexdigest(),','.join(map(str,h))))
    manifest={'format':'EFGY-OEM-RESTORE-1','model':'EH433M3v1','oem_version':'3.7.1','raw_sha256':hashlib.sha256(raw).hexdigest(),'source_hex_sha256':hashlib.sha256(Path(path).read_bytes()).hexdigest(),'hex_bytes':len(hexf),'hex_md5':hashlib.md5(hexf).hexdigest(),'power_fail_safe_during_commit':False}
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')

def bundle(converter,out):
    out=Path(out);manifest=json.loads((out/'manifest.json').read_text());data=Path(converter).read_bytes()
    manifest['converter_sha256']=hashlib.sha256(data).hexdigest()
    path=out/'oem-3.7.1-network-restore.zip'
    with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED) as z:
        z.writestr('manifest.json',json.dumps(manifest,indent=2)+'\n');z.writestr('converter.efu',data)
        z.write(out/'oem-pic.bin','oem-pic.bin');z.write(out/'restore-header.bin','restore-header.bin')
    return path

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--oem');p.add_argument('--converter');p.add_argument('--out',default='build/oem-restore');a=p.parse_args()
    if a.converter:print(bundle(a.converter,a.out))
    else:prepare(a.oem,a.out)
