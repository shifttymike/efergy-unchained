#!/usr/bin/env python3
"""Build a bounded stock-loader bootstrap carrier for the custom firmware."""
import argparse, hashlib, json, struct, zlib
from pathlib import Path
from package_firmware import read_hex, write_hex, APP_BASE, BOOT_BASE, APP_SIZE, MODEL

BLOB_BASE = 0x6000
ENTRY_STUB = 0x1cbc0

def encode(data):
    out=bytearray(); i=0
    while i<len(data):
        if data[i]==255:
            n=1
            while n<128 and i+n<len(data) and data[i+n]==255:n+=1
            out.append(128+n-1);i+=n
        else:
            start=i;i+=1
            while i<len(data) and i-start<128 and data[i]!=255:i+=1
            out.append(i-start-1);out.extend(data[start:i])
    return bytes(out)

def decode(data, size):
    out=bytearray();i=0
    while i<len(data):
        token=data[i];i+=1;n=(token&127)+1
        if token&128:out.extend(b'\xff'*n)
        else:
            if i+n>len(data):raise ValueError('truncated literal')
            out.extend(data[i:i+n]);i+=n
        if len(out)>size:raise ValueError('decoded overflow')
    if len(out)!=size:raise ValueError('decoded length')
    return bytes(out)

def prepare(factory, package, directory):
    memory=read_hex(factory);app=Path(package).read_bytes()
    if len(app)!=64+APP_SIZE or app[:8]!=b'EFGYOTA1':raise ValueError('bad package')
    if (struct.unpack_from('<III',app,8)!=(MODEL,APP_BASE,APP_SIZE)
        or struct.unpack_from('<I',app,20)[0]!=zlib.crc32(app[64:])
        or struct.unpack_from('<I',app,56)[0]!=zlib.crc32(app[:56])
        or app[63]!=0xa5):raise ValueError('invalid package header/CRC')
    for address,expected in [(0,'00efe6f0'),(8,'04ef02f0'),(0x18,'0cef02f0')]:
        if bytes(memory.get(address+i,255) for i in range(4))!=bytes.fromhex(expected):
            raise ValueError('incompatible factory vectors')
    if app[64:]!=bytes(memory.get(a,255) for a in range(APP_BASE,BOOT_BASE)):
        raise ValueError('package/factory mismatch')
    loader=bytes(memory.get(a,255) for a in range(BOOT_BASE,0x20000))
    streams=[encode(app),encode(loader)]
    assert decode(streams[0],len(app))==app
    assert decode(streams[1],len(loader))==loader
    blob=b''.join(streams)
    if BLOB_BASE+len(blob)>ENTRY_STUB:raise ValueError('bootstrap data exceeds OEM application')
    out=Path(directory);out.mkdir(parents=True,exist_ok=True)
    (out/'transition-blob.bin').write_bytes(blob)
    (out/'transition_image.h').write_text(
        '#define TRANSITION_BLOB_BASE 0x%xul\n'%BLOB_BASE+
        '#define TRANSITION_APP_PACKED %dul\n'%len(streams[0])+
        '#define TRANSITION_LOADER_PACKED %dul\n'%len(streams[1])+
        '#define TRANSITION_LOADER_FULL_CRC 0x%08xul\n'%zlib.crc32(loader)+
        '#define TRANSITION_LOADER_CRC 0x%08xul\n'%zlib.crc32(loader[:-8]))
    (out/'transition-inputs.json').write_text(json.dumps({
        'factory_sha256':hashlib.sha256(Path(factory).read_bytes()).hexdigest(),
        'package_sha256':hashlib.sha256(app).hexdigest(),
        'blob_bytes':len(blob),'blob_end':hex(BLOB_BASE+len(blob)),
        'initial_loader_replacement_power_fail_safe':False},indent=2)+'\n')

def combine(bootstrap,directory):
    out=Path(directory);memory=read_hex(bootstrap)
    if any(a<APP_BASE or a>=BLOB_BASE for a in memory):raise ValueError('bootstrap code outside bank-zero reservation')
    blob=(out/'transition-blob.bin').read_bytes()
    memory.update({BLOB_BASE+i:v for i,v in enumerate(blob)})
    # Stock loader rewrites this entry to its own reset vector and saves this
    # GOTO 0x400 at 0x1cbc0. The two interrupt vectors are already final.
    for address,target in [(0,0x400),(8,0x408),(0x18,0x418)]:
        word=target//2
        encoded=bytes([word&255,0xef,(word>>8)&255,0xf0|((word>>16)&15)])
        memory.update({address+i:v for i,v in enumerate(encoded)})
    write_hex(out/'efergy-oem-transition.hex',memory)
    size=(out/'efergy-oem-transition.hex').stat().st_size
    if size+0x1000>0x4f000:raise ValueError('OEM HEX staging would overwrite saved config')
    return size

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--factory');p.add_argument('--package');p.add_argument('--bootstrap');p.add_argument('--out',default='build/transition');a=p.parse_args()
    if a.bootstrap:print('Transition HEX bytes:',combine(a.bootstrap,a.out))
    else:prepare(a.factory,a.package,a.out)
