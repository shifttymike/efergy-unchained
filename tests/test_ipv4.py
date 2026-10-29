"""Compile and exercise the exact vendor IPv4 helper without unrelated MCU code."""
import re
import subprocess
from pathlib import Path

root=Path(__file__).resolve().parents[1]
source=(root/"vendor/microchip/src/Helpers.c").read_text()
start=re.search(r"^BOOL StringToIPAddress\(BYTE\* str, IP_ADDR\* IPAddress\)\s*\{",source,re.M).start()
end=source.index("\n}",start)+2
prefix='''#include <stdint.h>
#include <assert.h>
#include <stdio.h>
typedef uint8_t BYTE,BOOL;
typedef union {uint32_t Val;uint8_t v[4];} DWORD_VAL,IP_ADDR;
#define TRUE 1
#define FALSE 0
'''
test='''
int main(void) {
    IP_ADDR address={0};
    const char *bad[]={"","1","1.2","1.2.3","1.2.3.","1.2.3.4.","256.1.2.3","broker.local","1.2.3.4.nip.io"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);++i)assert(!StringToIPAddress((BYTE *)bad[i],&address));
    assert(StringToIPAddress((BYTE *)"192.168.23.22",&address)&&address.Val==0x1617a8c0ul);
    assert(StringToIPAddress((BYTE *)"0.0.0.0",&address)&&address.Val==0);
    assert(StringToIPAddress((BYTE *)"255.255.255.255",&address)&&address.Val==0xfffffffful);
    puts("IPv4: exact vendor helper accepts full quads and rejects partial numeric hosts");
}
'''
out=root/"build/test_ipv4.c"
out.write_text(prefix+source[start:end]+test)
subprocess.run(["clang","-std=c99","-Wall","-Wextra","-Werror","-fsanitize=address,undefined",str(out),"-o",str(out.with_suffix(""))],check=True)
subprocess.run([str(out.with_suffix(""))],check=True)
