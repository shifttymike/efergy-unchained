# OEM conversion bench results

Target tested here: EH433M3v1, OEM 3.7.1, PIC18F67J60 and 512 KiB
SST25VF040B. This is experimental; retain ICSP recovery and stable external power.

The dedicated local server is `../oem-installer`, deployed at
`192.168.23.30:/opt/efergy-oem-20261029`. Wildcard `*.sensornet.info` DNS directs
the OEM hub there. No request allowlist is used. Observation mode is the default.
The SSL compatibility frontend handles SSLv3/RC4-MD5; the clean firmware does not
need that legacy crypto stack.

The OEM host name `2A.A9.A5.5410EC.h3.sensornet.info` represents the real MAC
`5410ECA5A92A`. The command hash uses the latter order. This is independently
verified against actual telemetry request hashes. The authenticated `HEXFILE`
response has caused real requests for update metadata and the image. The first
307384-byte download was recovered from SPI and matched the served file exactly,
including its MD5. Metadata uses `URL: host/firmware.hex` and `MD5: digest`.

The first OEM self-reinstall test failed after erasing its application. The
incoming stock dump already contained `GOTO 0x1cc00` at address zero; the OEM
handler at `0x1e154–0x1e180` returns failure for this exact case. A valid carrier
must supply its **application entry** at zero. The OEM loader replaces that with
its own jump and saves the application jump at `0x1cbc0`. Do not serve a PICkit
factory HEX unchanged. The original application also uses routines in
`0x1fc00–0x1fff7`; these must be retained for an OEM self-reinstall test.

## Building the clean conversion carrier

From `firmware-clean`, run `make oem-transition`. It builds the ordinary factory
image/package, compresses the new application and loader using bounded literal/FF
runs, compiles a converter below `0x6000`, and assembles an OEM-loadable carrier.
The embedded streams begin at `0x6000` and must end before the OEM entry stub at
`0x1cbc0`. The packager checks that ASCII HEX staging at SPI `0x1000` ends before
the saved clean configuration at `0x4f000`.

The converter validates the embedded loader before replacing anything. It erases
old update journals and the alternate slot, stages and verifies the new clean
application in slot zero, commits its header and confirmed journal record, then
programs and verifies the new permanent loader and configuration words. On reset,
the new loader installs that active application. Configuration and web resource
sectors are outside the converter's write range. Restoring OEM itself may format
SPI storage; keep the private full-flash recovery dumps before that bench step.

**Replacing the OEM loader is not power-fail safe.** A power cut during that
one-time operation can require PICkit recovery. Subsequent clean firmware updates
use the permanent loader and two-slot recovery scheme. Host packaging tests do not establish electrical or instruction-level correctness.
The corrected OEM self-reinstall booted and resumed reporting without ICSP
intervention. The clean 0.6.2 carrier then completed the full network conversion:
OEM downloaded it from the LXC, its loader installed the converter, the converter
staged the clean application and replaced the loader, and clean 0.6.2 booted with
a valid confirmed journal. Radio decoding and MQTT connection/publications were
verified after restoring the private saved configuration through POST /config.
A subsequent software reboot preserved the confirmed journal and saved settings;
readings resumed. An exact-topic broker subscription confirmed its retained state
and online availability, including a later updated current/power reading.
This is a successful test on one EH433M3v1 / OEM 3.7.1 unit, not validation of other
models or firmware versions. No physical power-cut test has been performed.

The public release now includes the explicitly requested original OEM PIC firmware
as a separate artifact. MAC EEPROM images, broker credentials, private SPI dumps
and certificate keys remain excluded.
