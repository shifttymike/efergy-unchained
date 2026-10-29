import gzip,json,struct,zlib
from pathlib import Path
blob=Path("build/web-assets.bin").read_bytes()
magic,version,encoding,length,crc,reserved,commit=struct.unpack("<4sBBHI3sB",blob[:16])
assert (magic,version,encoding,reserved,commit)==(b"WEB2",1,1,b"\0"*3,0xa5)
assert len(blob)==length+16<=8192 and zlib.crc32(blob[16:])==crc
assert gzip.decompress(blob[16:])==Path("assets/index.html").read_bytes()
print("UI image: gzip roundtrip, length, CRC and committed header passed")
