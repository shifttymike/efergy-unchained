# Custom firmware 0.6.3

| File | Use |
| --- | --- |
| `custom-0.6.3-from-oem.hex` | OEM updater image for Docker-based OEM-to-custom conversion. |
| `custom-0.6.3-update.efu` | Update a gateway already running custom firmware. |
| `custom-0.6.3-pickit.hex` | Full PICkit image for OEM/fresh devices or recovery. |
| `custom-0.6.3.bin` | Raw PIC image for inspection; not the preferred programming file. |

To convert from OEM, copy `custom-0.6.3-from-oem.hex` to
`oem-server/images/firmware.hex` and follow the OEM-to-custom steps in
[`docs/switching.md`](../../docs/switching.md#oem-371-to-custom).

To update custom firmware over HTTP:

```sh
python3 tools/upload_firmware.py http://GATEWAY_IP \
  firmware/custom/custom-0.6.3-update.efu --reboot
```

Do not use the OEM conversion image as a custom OTA package.
