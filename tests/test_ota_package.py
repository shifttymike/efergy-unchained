"""Check the real linked images and reject incompatible/corrupt OTA packages."""
import sys,struct,zlib
from pathlib import Path
sys.path.insert(0,str(Path('tools').resolve()))
from package_firmware import read_hex,payload,header,APP_BASE,BOOT_BASE,APP_SIZE,MODEL
from upload_firmware import validate
app=read_hex('build/efergy.hex');boot=read_hex('build/bootloader.hex');factory=read_hex('build/efergy-factory.hex')
pkg=Path('build/efergy-update.efu').read_bytes();assert validate(pkg)==pkg
assert pkg[64:]==payload(app)
assert all(factory[a]==v for a,v in boot.items())
assert all(factory[a]==v for a,v in app.items() if APP_BASE<=a<BOOT_BASE)
assert not any(APP_BASE<=a<BOOT_BASE for a in boot)
assert not any(a<APP_BASE or BOOT_BASE<=a<0x1fff8 for a in app)
def jump(a):
    w0=factory[a]|factory[a+1]<<8;w1=factory[a+2]|factory[a+3]<<8
    assert w0&0xff00==0xef00 and w1&0xf000==0xf000
    return (((w1&0xfff)<<8)|(w0&255))*2
assert jump(0)==BOOT_BASE and jump(8)==APP_BASE+8 and jump(0x18)==APP_BASE+0x18
for i in [0,8,12,16,20,56,63,64,len(pkg)-1]:
    p=bytearray(pkg);p[i]^=1
    try:validate(p)
    except ValueError:pass
    else:raise AssertionError(f'accepted corrupt byte {i}')
for p in [pkg[:-1],pkg+b'x']:
    try:validate(p)
    except ValueError:pass
    else:raise AssertionError('accepted bad size')
# Confirm the silicon unlock sequence in the actual linked bootloader bytes.
symbols=Path('build/bootloader.sym').read_text().splitlines()
a=int(next(s for s in symbols if s.startswith('_unlock ')).split()[1],16)
assert bytes(boot[a+i] for i in range(12))==bytes.fromhex('55 0e a7 6e aa 0e a7 6e a6 82 00 00')
print('OTA package: real linker region separation, permanent vectors, silicon unlock bytes, model/size/CRC validation and factory merge passed')
# XC8 ABI regression: constant helpers assume the upper table-pointer bank.
# This failed on silicon despite correct host journal logic.
sym={parts[0]:int(parts[1],16) for line in symbols if len(parts:=line.split())>=2 and all(c in '0123456789abcdefABCDEF' for c in parts[1])}
bank=sym['__smallconst']>>16
assert bytes(boot[sym['_table_bank']+i] for i in range(4))==bytes([bank,0x0e,0xf8,0x6e])
def branch_bytes(target,opcode):
    return (opcode|((target>>1)&255)).to_bytes(2,'little')+(0xf000|((target>>9)&4095)).to_bytes(2,'little')
for name in ['main','boot_pic_read','boot_pic_row']:
    start,end=sym['_'+name],sym['__end_of_'+name]
    code=bytes(boot.get(i,255) for i in range(start,end))
    calls=[branch_bytes(sym['_table_bank'],op) for op in [0xec00,0xef00]]
    assert any(c in code for c in calls),f'{name} does not restore constant bank'
print('Bootloader constant-bank ABI: linked bank selection at startup and after raw PIC table access passed')
