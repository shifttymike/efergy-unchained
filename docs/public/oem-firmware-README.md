# OEM 3.7.1 images

These OEM files are separate from the GPL-licensed project code. Confirm
redistribution rights before publishing them.

| File | Use |
| --- | --- |
| `oem-3.7.1-pickit.hex` | Full stock PIC image for PICkit restoration. |
| `oem-3.7.1-update.hex` | Stock OEM application update image. |
| `oem-3.7.1-http-update.hex` | SSL-client bypass patch for the Docker update server. |
| `oem-3.7.1-http-pickit.hex` | Full PICkit image with the same patch. |
| `oem-3.7.1-http-patch.json` | Patch addresses and image hashes. |
| `oem-3.7.1-from-custom.zip` | Restore OEM from custom firmware with `tools/restore_oem.py`. |
| `oem-3.7.1.bin` | Raw PIC image used by the restore converter. |

## Disable the OEM SSL client

The patched image differs in 12 PIC bytes. It bypasses the SSL client branch and
uses HTTP port 80 for affected services. It preserves `uk.pool.ntp.org` and the
OEM `h3`, `keys` and `update` hostnames under `sensornet.info`. The patch has not
been validated on hardware.

Copy `oem-3.7.1-http-update.hex` to `oem-server/images/firmware.hex`, then follow
[`docs/switching.md`](../../docs/switching.md#oem-371-to-http-only-oem).

## Return from custom firmware to OEM

```sh
python3 tools/restore_oem.py http://GATEWAY_IP \
  firmware/oem/oem-3.7.1-from-custom.zip --commit
```

Keep stable external power connected throughout the final conversion. It is not
power-fail safe and may erase custom SPI configuration. See the
[restoration steps](../../docs/switching.md#custom-to-oem-371).
