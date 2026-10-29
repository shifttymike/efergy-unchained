#!/usr/bin/env python3
"""Install the pinned official XC8 archive into this project only."""
from pathlib import Path
import hashlib
import tarfile
import urllib.request

ROOT=Path(__file__).resolve().parents[1]
URL="https://ww1.microchip.com/downloads/aemDocuments/documents/DEV/ProductDocuments/SoftwareTools/xc8-v4.00-full-install-macos-x64.tar.xz"
SHA256="dca195a9d62f501edba2e83d3a492f6c843f779ee30ae3512e7b5d4d75a96519"
archive=ROOT/".toolchain/xc8-v4.00-macos.tar.xz"
archive.parent.mkdir(exist_ok=True)
if not archive.exists():
    partial=archive.with_suffix(".part")
    urllib.request.urlretrieve(URL,partial)
    partial.replace(archive)
if hashlib.file_digest(archive.open("rb"),"sha256").hexdigest()!=SHA256:
    raise SystemExit("Compiler archive hash mismatch; refusing to extract")
if not (archive.parent/"xc8-v4.00/bin/xc8-cc").exists():
    with tarfile.open(archive) as tar:tar.extractall(archive.parent,filter="data")
print(archive.parent/"xc8-v4.00/bin/xc8-cc")
