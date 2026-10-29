#!/usr/bin/env python3
"""Network-only custom-to-OEM switch using a checked, two-stage restore bundle."""
import argparse,hashlib,json,struct,subprocess,sys,tempfile,time,urllib.error,urllib.request,zipfile,zlib
from pathlib import Path
from upload_firmware import validate

def load_bundle(path):
 with zipfile.ZipFile(path) as z:
  manifest=json.loads(z.read('manifest.json'));converter=z.read('converter.efu');raw=z.read('oem-pic.bin');header=z.read('restore-header.bin')
 if manifest['format']!='EFGY-OEM-RESTORE-1' or manifest['model']!='EH433M3v1':raise ValueError('unsupported restore bundle')
 validate(converter)
 if hashlib.sha256(converter).hexdigest()!=manifest['converter_sha256'] or hashlib.sha256(raw).hexdigest()!=manifest['raw_sha256']:raise ValueError('bundle checksum mismatch')
 if len(raw)!=131072 or len(header)!=64 or header[:8]!=b'EFGYOEM1' or zlib.crc32(header[:60])!=struct.unpack_from('<I',header,60)[0] or zlib.crc32(raw)!=struct.unpack_from('<I',header,16)[0]:raise ValueError('bad OEM image/header')
 return manifest,converter,raw,header

def main():
 p=argparse.ArgumentParser();p.add_argument('device');p.add_argument('bundle');p.add_argument('--commit',action='store_true',help='after verification, replace the loader and install OEM');p.add_argument('--commit-only',action='store_true',help='commit a previously staged image');a=p.parse_args()
 manifest,converter,raw,header=load_bundle(a.bundle);base=a.device.rstrip('/');opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))
 def request(path,body=None):
  req=urllib.request.Request(base+path,data=body,headers={'Content-Type':'application/octet-stream'} if body is not None else {})
  with opener.open(req,timeout=35) as r:return json.load(r)
 def wait_state(wanted,seconds=180):
  deadline=time.monotonic()+seconds
  while time.monotonic()<deadline:
   s=request('/restore/oem')
   if s['error']:raise RuntimeError('OEM staging error '+str(s['error']))
   if s['state']==wanted:return s
   time.sleep(.3)
  raise TimeoutError('OEM staging timed out')
 if not a.commit_only:
  status=request('/status')
  if not status.get('converter'):
   with tempfile.TemporaryDirectory() as directory:
    image=Path(directory)/'converter.efu';image.write_bytes(converter)
    subprocess.run([sys.executable,str(Path(__file__).with_name('upload_firmware.py')),base,str(image),'--reboot'],check=True)
  deadline=time.monotonic()+240
  while time.monotonic()<deadline:
   try:
    if request('/status').get('converter'):
     request('/restore/oem',header);break
   except (OSError,ValueError):pass
   time.sleep(3)
  else:raise TimeoutError('converter did not become ready/confirmed')
  wait_state(3)
  for offset in range(0,len(raw),256):
   request('/restore/oem/data',struct.pack('<I',offset)+raw[offset:offset+256])
   if offset%8192==0:print(f'OEM image uploaded {offset}/{len(raw)}',flush=True)
  wait_state(6)
  print('OEM PIC image verified in unused SPI space. Custom loader/recovery slots are still intact.',flush=True)
 else:wait_state(6,5)
 if a.commit or a.commit_only:
  request('/restore/oem/commit',b'')
  print('OEM conversion committed. Keep external power connected; allow several minutes for staging/install and DHCP.',flush=True)
 else:print('Run again with --commit-only to install OEM. Converter remains installed until then.',flush=True)
if __name__=='__main__':main()
