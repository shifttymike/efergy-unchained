# Flashing tools

Run these Python 3 tools from the release root:

* `upload_firmware.py URL PACKAGE.efu --reboot` — update custom firmware.
* `restore_oem.py URL firmware/oem/oem-3.7.1-from-custom.zip --commit` — restore
  OEM from custom firmware.
* `patch_oem_http.py OEM.hex --out DIR` — recreate the HTTP-only OEM patch from
  the verified original OEM HEX.

Keep stable external power connected and leave PICkit target power disabled.
See [`docs/switching.md`](../docs/switching.md) for complete steps.
