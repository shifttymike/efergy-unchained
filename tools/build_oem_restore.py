#!/usr/bin/env python3
"""Build a one-off restore application without altering the normal build."""
import argparse,subprocess,sys
from pathlib import Path
from package_oem_restore import prepare,bundle
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('oem');p.add_argument('--xc8',default=str(ROOT/'.toolchain/xc8-v4.00/bin/xc8-cc'));p.add_argument('--dfp',default='/Applications/microchip/mplabx/v6.20/packs/Microchip/PIC18F-J_DFP/1.7.159/xc8');a=p.parse_args()
 out=ROOT/'build/oem-restore';prepare(a.oem,out)
 stack=['ARP','DHCP','DNS','ETH97J60','Helpers','ICMP','IP','StackTsk','TCP','Tick','UDP','Delay']
 sources=sorted(x for x in (ROOT/'src').glob('*.c') if x.name!='radio_sampler.c')+sorted((ROOT/'src').glob('*.S'))+[ROOT/f'vendor/microchip/src/{x}.c' for x in stack]
 cmd=[a.xc8,'-mcpu=18F67J60','-mdfp='+a.dfp,'-O2','-std=c99','-mwarn=-3','-mcodeoffset=0x400','-mrom=0x400-0x1cbff,0x1fff8-0x1ffff','-DOEM_RESTORE_BUILD=1','-DEFERGY_FIRMWARE_VERSION="0.6.3-oem"']
 for directory in [out,ROOT/'build',ROOT/'include',ROOT/'vendor/microchip/include']:cmd+=['-I'+str(directory)]
 subprocess.run(cmd+list(map(str,sources))+['-o',str(out/'converter.hex')],check=True,cwd=ROOT)
 subprocess.run([sys.executable,str(ROOT/'tools/package_firmware.py'),str(out/'converter.hex'),'--version','0.6.3-oem','--out',str(out)],check=True)
 print(bundle(out/'efergy-update.efu',out))
if __name__=='__main__':main()
