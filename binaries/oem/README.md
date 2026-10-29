# OEM firmware 3.7.1

| File | Purpose |
| --- | --- |
| `oem-3.7.1-pickit.hex` | Full stock OEM image for PICkit restoration. |
| `oem-3.7.1-update.hex` | Stock OEM application image for the OEM updater. |
| `oem-3.7.1-http-pickit.hex` | Full PICkit image with the SSL client path patched to use HTTP. |
| `oem-3.7.1-http-update.hex` | OEM updater image with the SSL client path patched to use HTTP. |
| `oem-3.7.1-from-custom.zip` | OEM restoration package for `tools/restore_oem.py`. |
| `oem-3.7.1.bin` | Raw PIC image used by the restoration converter; not a PICkit image. |

The HTTP-patched images use port 80 for affected services and preserve the OEM
hostnames and NTP address. These OEM images remain separately copyrighted; see
[`docs/public/LICENSE-SCOPE.md`](../../docs/public/LICENSE-SCOPE.md) before redistribution.
