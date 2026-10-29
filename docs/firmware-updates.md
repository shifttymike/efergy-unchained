# Firmware updates — development build 0.6.0

**Built and host-tested; not flashed or hardware-validated.** The deployed device
continues running 0.5.1. Do not advertise this as a proven stock-device Ethernet
installer. The OEM transition/bootstrap and its initial power-loss risks remain
separate work; reserve ICSP recovery for that bench test.

## Images and installation

`make` produces:

- `build/efergy-factory.hex`: permanent loader, low vectors, application and
  configuration words. Install via PICkit for the first loader test, with external
  power only. This is NOT an HTTP update file.
- `build/efergy-update.efu`: a 64-byte versioned header and a fixed-length, padded
  application image. This is the only format accepted by the HTTP updater.
- `build/firmware-package.json`: identities, CRC and image hashes.

The linker places the application reset at `0x400`, interrupt vectors at
`0x408` / `0x418`, and all application code below `0x1cc00`. The permanent reset
at zero jumps to the loader at `0x1cc00`; fixed interrupt jumps occupy `0x8`
and `0x18`. The entire `0x0000–0x03ff` erase block, loader region
`0x1cc00–0x1ffff`, and configuration words are outside ordinary updates.
Protection is enforced by the image packer, linker and loader write bounds;
it is not a security boundary against deliberately malicious application code.

The application payload is 116,736 bytes, including erased-byte padding. The
header identifies the hardware/layout (0x18676069), load address, length,
application version and CRC32, plus a header CRC and commit byte. Validation
checks exact layout/model/length and both CRCs. CRCs detect damage; updates are
not signed or authenticated. HTTP/configuration remain intended for a trusted
local network. Do not publish these endpoints to the Internet.

SPI partitions:

| Range | Purpose |
|---|---|
| `0x00000–0x1cfff` | Application slot 0 (64-byte header + payload + padding) |
| `0x1d000–0x39fff` | Application slot 1 |
| `0x3a000–0x3afff` | Journal sector A |
| `0x3b000–0x3bfff` | Journal sector B |
| `0x3c000–0x4efff` | Unallocated; updater does not write here |
| `0x4f000`, `0x50000` | Existing configuration sectors, preserved |
| `0x51000–0x52fff` | Existing web assets, managed by application |

The update partitions replace obsolete OEM staging/log contents. Original dumps
remain the recovery source. MAC EEPROM is neither erased nor written.

## Upload from a computer

After the factory loader is installed and its first boot completes:

```sh
python3 tools/upload_firmware.py http://192.168.23.13 build/efergy-update.efu
# Optional: request reboot automatically after successful staging/verification:
python3 tools/upload_firmware.py http://192.168.23.13 build/efergy-update.efu --reboot
```

Use the device's actual DHCP address. The compiled fallback remains
`192.168.123.1/24`, overridden by existing saved settings.

The tool checks the entire package locally before transmission. It starts the
inactive-slot erase, uploads sequential 256-byte chunks, waits for flash readback,
requests complete-image verification, and waits for a durable pending journal
record. There are no blind retries of a failed chunk. After an interrupted upload,
restart from the beginning; the running/active image remains intact. An inactive
slot being overwritten is not available for rollback until verified and committed.
The backend runs cooperatively; radio/MQTT continue during staging. No additional
TCP socket or whole-image RAM buffer is allocated. A browser upload UI is not
included in this build; use this tool or the endpoints below.

## Installation and recovery

On the first factory boot, the loader checks the internal application's CRC
against its compiled factory identity, snapshots it into SPI slot 0, verifies it,
and initializes the journal. Keep power connected; there is no HTTP server while
the loader runs. Yellow stays on during loader work. A loader failure leaves
solid yellow, with the loader intact; power cycling retries the operation.

For an update, the loader validates the staged slot before erasing any application
page. It erases 1,024-byte pages and writes/verifies 64-byte rows. A power cut
while programming leaves the durable pending record, so the next boot repeats
installation from SPI. Once complete, a trial record is committed before starting
the application. The previous active slot is retained.

The trial confirms only after at least 45 seconds of responsive main-loop work,
ready web assets and a bound DHCP/static network. MQTT broker availability is not
required. The hardware watchdog is software-enabled in the application (nominal
~33-second postscale); the main loop services it. An unconfirmed trial also has a
120-second main-loop deadline. If reset occurs before confirmation, the loader
restores the previous active slot on the next boot. A power loss during rollback
leaves the trial record, so restoration repeats on the next boot.

There is no physical button. For manual recovery, power-cycle repeatedly so the
loader reaches application startup **three times without allowing 45 seconds of
application uptime between boots**. Allow each loader operation to finish before
the next power cut; avoid cycling during programming. On the third short confirmed
boot, the loader schedules the other slot if its header and full CRC are valid,
resets, installs it and gives it a trial boot. This is a network-independent
recovery path. If the other slot is invalid/empty, it continues the current image.
This sequence and LED behavior require hardware confirmation before publication.

The journal consists of 32-byte CRC-protected records, with the commit marker
written last. Full sectors rotate without erasing the latest committed record.
Incomplete records are ignored/skipped. Confirmation writes occur per boot/update,
not per telemetry reading. Both banks/slots being physically corrupt is outside
this software recovery guarantee and may require ICSP.

## HTTP API

All bodies have an explicit Content-Length; chunked transfer encoding is rejected.
The same single HTTP connection serves uploads and other UI traffic. A chunk owns
its request buffer until write/readback completes; a disconnect/timeout aborts the
session before that buffer can be reused.

| Request | Body / effect |
|---|---|
| `GET /update` | JSON state, error, offset, size, active slot, journal phase, ready_to_reboot |
| `POST /update` | Exactly 64 binary header bytes, `application/octet-stream`; erase inactive slot |
| `POST /update/data` | 4-byte little-endian sequential payload offset, then 1–256 binary bytes |
| `POST /update/commit` | Empty; verify complete payload and commit pending record |
| `POST /update/cancel` | Empty; cancel uncommitted upload (cannot undo a durable pending update) |
| `POST /update/rollback` | Empty; validate other slot and schedule it as a trial image |
| `POST /reboot` | Existing empty-body endpoint; installs a committed update on reboot |

States: 0 idle, 1/2 erasing, 3 awaiting data, 4/5 writing/readback,
6 complete-image verification, 7/8 header commit, 9–12 journal commit,
13 ready to reboot, 255 failed. Error: 1 flash command, 2 flash timeout,
3 readback, 4 image CRC, 5 journal allocation. Journal phases: 0 confirmed,
1 pending installation, 2 trial. Config saves/reboot are rejected while an upload
operation is active. A new begin can restart an abandoned state-3 upload.

## Validation

Host tests exercise the actual portable boot flow and updater, using NOR-flash
and internal-flash models with address/write constraints. They cover 178 cut
points during install and trial transition, 15 initial-snapshot interruption
points, all 33 rotation erase/write boundaries,
factory snapshot, invalid candidate rejection, previous-image restoration,
manual recovery, streamed upload/readback, offset/bounds checks, flash timeout,
abort and trial confirmation/deadline. Linker/package tests inspect real image
regions/vectors and the PIC 0x55/0xaa unlock sequence. Existing tests still run.
These tests do not emulate PIC instructions, electrical timing or a physical
power interruption. First-flash and initial snapshot interruption need bench tests.

The 29 October bench pass verified the initial snapshot, HTTP upload of a second
build, trial confirmation, persistence after reset, restoration of the previous
image when a trial was reset before confirmation, and recovery to the alternate
slot after three short boots. A real XC8 table-pointer bank assumption in the
upper loader was found and corrected; old 0.6.0 factory artifacts are superseded.
Those update/rollback tests used the corrected loader with applications 0.6.0 and
0.6.1. The exact 0.6.2 factory image subsequently booted and initialized a valid
confirmed journal during recovery. Physical power cuts have not been tested.

The separate OEM conversion is still under bench validation; see
[OEM transition](oem-transition.md). The OEM loader must not be assumed capable
of directly replacing its reserved region with our factory HEX. Keep PICkit
recovery and independent external power available; do not enable programmer power.
