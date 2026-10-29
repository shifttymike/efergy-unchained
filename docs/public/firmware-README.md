# Firmware images

Choose the image for the gateway's current firmware:

* `custom/` — custom factory, OTA and OEM-to-custom images.
* `oem/` — stock OEM, HTTP-only OEM patch and custom-to-OEM restoration images.

Each folder has a short file guide. See [`docs/switching.md`](../docs/switching.md)
for installation steps. Use `*-pickit.hex` for PICkit; the other HEX and EFU
files are for the update paths described in the guides. Verify files with the
root `SHA256SUMS` before use.
