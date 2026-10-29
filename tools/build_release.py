#!/usr/bin/env python3
"""Assemble only the explicit public release inputs; exclude private bench data."""
import argparse,hashlib,json,shutil,tempfile,zipfile
from pathlib import Path
from package_firmware import read_hex
from package_oem_restore import stock_images
from patch_oem_http import build as build_oem_http_patch
ROOT=Path(__file__).resolve().parents[1]
VERSION='0.6.3'
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def copy(source,target):target=Path(target);target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
def main():
 p=argparse.ArgumentParser();p.add_argument('--oem',required=True);p.add_argument('--server');p.add_argument('--validation',help='public validation JSON');p.add_argument('--replace',action='store_true');a=p.parse_args()
 server=Path(a.server) if a.server else next(x for x in [ROOT.parent/'oem-installer',ROOT.parent/'oem-server'] if x.is_dir())
 out=ROOT/'dist'/('efergy-'+VERSION);out.parent.mkdir(exist_ok=True)
 if out.exists() and (not a.replace or not (out/'release.json').is_file()):raise SystemExit('release exists; use --replace for an existing generated release')
 with tempfile.TemporaryDirectory(prefix='release-',dir=out.parent) as temporary:
  stage=Path(temporary)/out.name;stage.mkdir()
  raw,_=stock_images(a.oem);oem=stage/'firmware/oem';custom=stage/'firmware/custom';oem.mkdir(parents=True);custom.mkdir(parents=True)
  copy(a.oem,oem/'oem-3.7.1-pickit.hex');(oem/'oem-3.7.1.bin').write_bytes(raw)
  copy(ROOT/'build/oem-restore/oem-application.hex',oem/'oem-3.7.1-update.hex')
  copy(ROOT/'build/oem-restore/oem-3.7.1-network-restore.zip',oem/'oem-3.7.1-from-custom.zip')
  build_oem_http_patch(a.oem,oem)
  copy(ROOT/'build/efergy-factory.hex',custom/f'custom-{VERSION}-pickit.hex')
  memory=read_hex(ROOT/'build/efergy-factory.hex');(custom/f'custom-{VERSION}.bin').write_bytes(bytes(memory.get(x,255) for x in range(0x20000)))
  copy(ROOT/'build/efergy-update.efu',custom/f'custom-{VERSION}-update.efu')
  copy(ROOT/'build/transition/efergy-oem-transition.hex',custom/f'custom-{VERSION}-from-oem.hex')
  # Explicit folder selection intentionally omits .git, .toolchain, build,
  # artifacts, recovery, sessions, credentials and existing private bench docs.
  ignore=shutil.ignore_patterns('.git','__pycache__','.DS_Store','*.pyc','*.o','*.p1','*.d','*.lst','*.sym','*.elf')
  for name in ['src','include','bootloader','bootstrap','assets','tests','tools','vendor']:
   shutil.copytree(ROOT/name,stage/'source'/name,ignore=ignore)
  copy(ROOT/'Makefile',stage/'source/Makefile')
  (stage/'docs').mkdir()
  for name in ['building.md','hardware.md','oem-server.md','switching.md','usage.md','validation.md','validation.json','THIRD-PARTY-NOTICES.md']:
   copy(ROOT/'docs/public'/name,stage/'docs'/name)
  copy(ROOT/'docs/public/docs-README.md',stage/'docs/README.md')
  readme=(ROOT/'docs/public/README.md').read_text()
  (stage/'README.md').write_text(readme)
  shutil.copytree(ROOT/'docs/public/images',stage/'images')
  copy(ROOT/'LICENSE',stage/'LICENSE')
  copy(ROOT/'docs/public/LICENSE-SCOPE.md',stage/'LICENSE-SCOPE.md')
  copy(ROOT/'docs/public/THIRD-PARTY-NOTICES.md',stage/'THIRD-PARTY-NOTICES.md')
  copy(ROOT/'docs/public/source-README.md',stage/'source/README.md')
  copy(ROOT/'docs/public/firmware-README.md',stage/'firmware/README.md')
  copy(ROOT/'docs/public/custom-firmware-README.md',custom/'README.md')
  copy(ROOT/'docs/public/oem-firmware-README.md',oem/'README.md')
  copy(ROOT/'docs/public/tools-README.md',stage/'tools/README.md')
  for name in ['upload_firmware.py','restore_oem.py','package_firmware.py','package_oem_restore.py','patch_oem_http.py']:
   copy(ROOT/'tools'/name,stage/'tools'/name)
  for name in ['Dockerfile','compose.yaml','server.py']:
   copy(server/name,stage/'oem-server'/name)
  for name in ['legacy-ssl','tests']:
   shutil.copytree(server/name,stage/'oem-server'/name,ignore=shutil.ignore_patterns('__pycache__','*.pyc','openssl-*.tar.gz','.DS_Store'))
  copy(ROOT/'docs/public/oem-server.md',stage/'oem-server/README.md')
  (stage/'oem-server/.gitignore').write_text('logs/*\nimages/*\nlegacy-ssl/openssl-*.tar.gz\n__pycache__/\n')
  metadata={'format':'EFGY-RELEASE-1','version':VERSION,'hardware':'EH433M3v1','oem_version':'3.7.1','project_license':'GPL-3.0-only (project-authored components; see LICENSE-SCOPE.md)','oem_original_sha256':sha(a.oem),'custom_factory_sha256':sha(ROOT/'build/efergy-factory.hex'),'oem_http_patch':json.loads((oem/'oem-3.7.1-http-patch.json').read_text()),'validation':json.loads(Path(a.validation).read_text()) if a.validation else {'status':'pending'},'private_eeprom_or_spi_dumps_included':False}
  (stage/'release.json').write_text(json.dumps(metadata,indent=2)+'\n')
  sums=''.join(sha(f)+'  '+f.relative_to(stage).as_posix()+'\n' for f in sorted(stage.rglob('*')) if f.is_file())
  (stage/'SHA256SUMS').write_text(sums)
  if out.exists():shutil.rmtree(out)
  shutil.move(stage,out)
 archive=Path(str(out)+'.zip')
 with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
  for file in sorted(out.rglob('*')):
   if file.is_file():z.write(file,Path(out.name)/file.relative_to(out))
 print(out);print(archive);print('ZIP SHA256',sha(archive))
if __name__=='__main__':main()
