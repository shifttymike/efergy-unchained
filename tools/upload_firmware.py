#!/usr/bin/env python3
"""Stage/verify a bounded .efu image; reboot only after an explicit --reboot."""
import argparse, json, struct, time, urllib.request, zlib
from pathlib import Path
from package_firmware import APP_BASE, APP_SIZE, MODEL

def validate(data):
    if len(data)!=64+APP_SIZE:raise ValueError('wrong package length')
    h=data[:64]
    if h[:8]!=b'EFGYOTA1' or struct.unpack_from('<III',h,8)!=(MODEL,APP_BASE,APP_SIZE) or h[63]!=0xa5:raise ValueError('incompatible package')
    if zlib.crc32(h[:56])!=struct.unpack_from('<I',h,56)[0] or zlib.crc32(data[64:])!=struct.unpack_from('<I',h,20)[0]:raise ValueError('checksum failure')
    return data

def main():
    ap=argparse.ArgumentParser();ap.add_argument('device',help='http://device-IP');ap.add_argument('image');ap.add_argument('--reboot',action='store_true');args=ap.parse_args();data=validate(Path(args.image).read_bytes());base=args.device.rstrip('/');opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))
    def request(path,body=None):
        req=urllib.request.Request(base+path,data=body,headers={'Content-Type':'application/octet-stream'} if path.startswith('/update') else {})
        with opener.open(req,timeout=35) as r:return json.load(r)
    def wait_state(wanted,timeout):
        until=time.monotonic()+timeout
        while time.monotonic()<until:
            s=request('/update')
            if s['error']:raise RuntimeError('device update error '+str(s['error']))
            if s['state']==wanted:return s
            time.sleep(.25)
        raise TimeoutError('device update operation timed out')
    request('/update',data[:64]);wait_state(3,120)
    for offset in range(0,APP_SIZE,256):
        request('/update/data',struct.pack('<I',offset)+data[64+offset:64+offset+256])
        if offset%8192==0:print(f'Uploaded {offset}/{APP_SIZE}',flush=True)
    request('/update/commit',b'');wait_state(13,120)
    print('Image verified and scheduled. Previous image retained for rollback.')
    if args.reboot:request('/reboot',b'');print('Reboot requested; allow up to several minutes for installation and trial confirmation.')
    else:print('Reboot the device to install (or pass --reboot).')
if __name__=='__main__':main()
