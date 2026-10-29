# Validation and limitations

Hardware: one EH433M3v1 board, PIC18F67J60 revision 3, RFM69W, SST25VF040B.
OEM version: 3.7.1. One e2 classic transmitter was used. This does not establish
compatibility with other Efergy boards, OEM versions or transmitter variants.

## Recorded tests

* Custom HTTP OTA, trial confirmation and persistence after reset.
* Unconfirmed-trial rollback and alternate-slot recovery after three short boots.
* OEM server command authentication, metadata parsing and exact SPI download.
* OEM self-reinstallation with a corrected application-entry vector.
* OEM-to-custom network conversion, confirmed journal, subsequent reboot and
  restored radio/MQTT operation.
* Custom-to-OEM network conversion: the temporary 0.6.3 converter received and
  verified the full OEM PIC image, restored the stock loader/application, and
  genuine OEM 3.7.1 SSLv3 reporting and radio-data POSTs resumed. No PICkit
  programming was used. The restored OEM then converted back to normal custom
  0.6.3 through the local server, with radio decoding and MQTT restored.

The release manifest records the final artifact hashes and bench result for this
release. OEM delivery logs indicate transfer completion, not installation success.
The custom-to-OEM test is verified separately from the existing PICkit restore.

Host tests exercise portable boot and update code, image bounds, CRC/header
checks, interrupted writes, journal rotation, initial snapshot cut points,
confirmation/deadlines, streamed upload/readback, HTTP framing and converter
carrier generation. The C OEM carrier generator is compared byte for byte with
the packaging tool. These are real code tests, but not a PIC instruction emulator.

A real XC8 constant-table bank assumption was discovered on hardware and fixed
in the permanent loader. Old pre-fix 0.6.0 factory images are superseded and are
not included in the release. Keep the protected loader/app linker separation.

No physical power-cut testing has been performed. The modeled interrupted-write
coverage does not make the one-time bootloader conversions power-fail safe.
Both firmware directions need stable external power during their commit phase.
Neither a CRC nor an OEM MD5 digest authenticates a firmware publisher.

MQTT is QoS 0, with retained state and discovery; no historical replay, NTP
readings or TLS are implemented. Estimated watts assume configured voltage and
power factor 1. RF noise, packet loss and network/server outages affect readings.
Five transmitter IDs fit the implementation; only one was exercised on hardware.
The HTTP server services one connection, so use serialized, moderate status polls.


The release converter was rebuilt after removing bench-specific emergency-MAC
and static-network defaults. Its restore implementation is unchanged from the
hardware test. Both released update binaries reproduce byte for byte when built
from the exported source. Project-authored custom firmware/support code is
GPL-3.0-only; third-party and OEM components remain separately licensed as
described by the release license scope.
