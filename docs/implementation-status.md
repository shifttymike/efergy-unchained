# Implementation status — firmware 0.5.1

The clean image is installed on the bench device. It receives the observed e2
classic transmitter, serves raw HTTP configuration, persists settings in SPI
flash, and publishes MQTT with retained Home Assistant discovery. The browser UI is served
from gzip data in SPI flash and polls status about every three seconds while visible. See [HTTP API](http-api.md).

## Implemented

- XC8 4.00 build using PIC18F-J DFP 1.7.159; 62,942 program bytes and
  3,294 bytes of ordinary RAM allocated by the linker. RAM remains the tight limit.
- Recovered RFM69 register profiles and bank-local assembly continuous receiver;
  bounded packet FIFO capture is also preserved. The e2 decoder validates the
  checksum, handles inversion and trailing capture noise, and reports current,
  transmitter ID, interval, learn mode and battery status. RSSI is sampled from
  RegRssiValue at receive entry and mapped to dBm in 0.5 dB steps.
- Microchip TCP/IP 5.42 adapted for XC8: internal Ethernet, ARP/IP, DHCP, DNS,
  TCP and ICMP; one HTTP socket and one MQTT socket in Ethernet packet RAM.
- Nonblocking 125 ms activity LEDs: yellow on radio capture, green on Ethernet
  RX/TX. GPIO colors/flashing were visually confirmed by the user; see [bench](bench.md).
- EEPROM-derived MAC, DHCP with a 30-second timeout, saved static fallback
  `192.168.23.6/24`, ARP probing/conflict withdrawal, and deferred network changes.
- Bounded HTTP GET status/config, URL-encoded POST config and POST reboot.
  `GET /` streams gzip HTML/JS from `0x51000–0x52fff` in 256-byte chunks.
  The compiled installer repairs interrupted asset writes and reuses unchanged
  assets after verification; config and asset writes are mutually excluded.
  Browser navigation headers fit bounded 511-byte lines and 2,048-byte totals.
  A shared main-loop workspace serves MQTT encoding and config staging to keep
  RAM use bounded; interrupts never touch it.
  Passwords are redacted; the saved MQTT username is returned by `/config`. Raw flash download is disabled in this image.
- Alternating CRC-protected configuration records at `0x4f000` and `0x50000`;
  asynchronous erase/program, verification and final commit marker. Identical
  updates avoid flash writes; failed saves retain the previous committed record.
  Version-1 records load with 230 V; version-2 records retain voltage. Both default
  the new device name to Efergy gateway; updates write version-3, 231-byte records
  while retaining existing MQTT credentials.
- MQTT 3.1.1 authentication, hostname resolution, reconnect/backoff, keepalive,
  retained availability/Last Will, readings, and per-transmitter Home Assistant
  current, battery, signal strength, estimated power, assumed-voltage and reporting-
  interval discovery. Device name is configurable and JSON escaped; stable IDs
  and topics are retained.
  The assumed voltage defaults to 230 V and is exposed through GET/POST `/config`
  as `voltage_v` (whole volts 1–500). Power is current × voltage, assuming PF=1.
  Five latest transmitter readings are cached in
  RAM; there is no historical offline outbox. No TLS or heap allocation.

## Verification

`make check` passes with address/undefined-behavior sanitizers on portable C:
radio decoder/synthetic PWM, MQTT framing, HTTP framing/config validation,
650 simulated storage reset boundaries, flash failures/timeouts, v1/v2-to-v3
credential-preserving migration, voltage bounds, power rounding/extremes, RSSI
formatting, MQTT session
and discovery JSON, network transitions, the actual vendor IPv4 helper, and HTTP reboot completion
when a client immediately closes its connection. A conservative instruction-path
audit also bounds the assembly sampler body to 77 PIC instruction cycles between
Timer3 flag clear and its next poll, below the 96-cycle reload interval.
Synthetic timing tests do not establish hardware RF timing. Asset tests cover
interrupted installation, commit-last ordering, boot reuse, corruption repair,
flash bounds/timeouts, locks, gzip validation and partial HTTP streaming.
The actual embedded script is tested with delayed/failed HTTP responses for
non-overlap, 3-second completion-based scheduling, retry backoff, recovery and
hidden-page pause. A small random delay separates independent viewers.

Hardware checks covered DHCP, EEPROM MAC and flash ID, live six-second radio
reports while networking runs, authenticated Mosquitto publishing, retained
Home Assistant payloads, credential refusal/recovery, DNS failure/recovery,
broker restart/reconnect, keepalive, persistent saves/reboots, static addressing,
malformed POST rejection, credential redaction and unchanged-update handling.
The final programmed image passed PICkit verification with target power disabled.
Firmware 0.3 hardware checks also verified live RSSI and estimated-power state,
all five retained discovery messages on the real broker, a POST change to 240 V
and MQTT recalculation, invalid-voltage rejection, then restoration to 230 V
and persistent credentials/configuration after reboot. See
[0.3 evidence](../artifacts/firmware-0.3.json).

Firmware 0.4 was verified in the in-app browser: live status polling, name save
and restoration, preserved MQTT credentials, six retained discovery records
including interval, and persistent configuration/UI after reboot. The served
gzip exactly matches the generated asset. See [0.4 evidence](../artifacts/firmware-0.4.json).

A fresh full SPI backup was read twice before configuration writes. Subsequent
full readback confirmed every byte outside the two configuration sectors was
unchanged; both committed slots independently passed CRC checks. See
[verification evidence](../artifacts/firmware-0.2.json). Storage power-loss tests
were simulated; no physical power-cut test was performed.

## Remaining work and limits

- The user confirmed the MQTT device appears in Home Assistant. New 0.3 entities
  have verified retained discovery on the real broker; their HA UI appearance
  has not been independently observed.
- Resolve intermittent checksum failures/reception gaps observed during the earlier 0.2
  integration run. The sampler timing overrun was corrected, but this alone does
  not establish reception reliability or identify all remaining causes. A 55-second
  quiet run produced four valid readings from eight capture attempts; the latter
  includes noise triggers, so this is not a measured packet-loss percentage. Latest
  capture bytes/bit counts are exposed in `/status`; retain the RF/logic analyzer
  follow-up before treating this image as production ready.
- Long-duration soak, deliberate physical power-cut recovery and production
  watchdog behavior. The development image has its watchdog disabled.
- Implement the other recovered radio profile's payload semantics and establish
  equivalence to every original sensor/protocol. Capturing its FIFO alone does
  not establish compatibility. RF reception is not claimed to be lossless.
- Estimated watts assume the configured voltage and power factor 1; neither is
  measured. No accumulated energy, NTP or persistent telemetry backlog is
  implemented. RSSI is the receiver indication, not calibrated field strength.
  Publishing is QoS 0.

## Device handover

The clean 0.5.1 image stays installed. DHCP currently assigns `192.168.23.13`;
static fallback is `.6`, with gateway/DNS `.254`. The user's broker at
`10.40.0.9:1883` remains configured and connected. Voltage was restored to 230 V
after the POST check; existing broker credentials were preserved. Original PIC and fresh SPI recovery images
remain preserved; see [recovery manifest](recovery-manifest.json).


Firmware 0.4.1 replaces fixed one-second polling with sequential ~3-second
polling, up to 0.5 s jitter, capped failure backoff and hidden-page pause. In a
60-second check with two simulated viewers (including page/config downloads),
all 40 requests succeeded; maximum response was 0.619 s and median 0.055 s.
Nine valid readings and nine MQTT state publications continued during the test.
See [0.4.1 evidence](../artifacts/firmware-0.4.1.json). Existing open pages must
reload to load the new script. The server still has one HTTP connection.


Firmware 0.4.2 advertises DHCP option 12 `efergyhub` in discovery, request and
renewal packets. RouterOS read-only lease inspection confirmed hostname
`efergyhub` on the bound lease at `.13`; MQTT and radio reception resumed.
See [0.4.2 evidence](../artifacts/firmware-0.4.2.json).


Firmware 0.5 adds a power-only Chart.js graph to the browser UI. Chart.js 4.5.1
loads from jsDelivr with an integrity check; the library is not packed into the
firmware. Each newly observed reading adds a point, retaining a rolling hour in
page memory. Reloading clears it. Browser receive times are used; missed readings
are not recovered, and gaps break the line. Existing status polling is unchanged.
The gzip page occupies 5,786 bytes (5,802 with its WEB2 header) of the 8 KiB
allocation; PIC RAM usage remains 3,310 bytes.

The full host checks pass, including history deduplication, counter wrap/reset,
gaps, voltage snapshots, one-hour pruning and CDN failure handling. Programming
and verification succeeded using external target power. The live browser rendered
the chart and accumulated 17 readings, with MQTT connected and 21 valid captures
out of 21 attempts at that observation. This is a brief bench check, not a soak.
The served gzip matched the built asset; configuration sequence 18 was preserved.
See [0.5 evidence](../artifacts/firmware-0.5.json) and
[UI screenshot](../artifacts/firmware-0.5-ui.png).


Firmware 0.5.1 collapses Configuration by default using a native details control.
The saved MQTT username is visible and editable; the password remains write-only
and masked on entry. The API JSON-escapes username values. Host checks cover
quotes/backslashes in usernames and ensure password values are never returned.
Configuration-response escaping uses the existing shared workspace; linked RAM
is 3,294 bytes and program space is 62,942 bytes.


## Unflashed build 0.5.2

The compiled default fallback IP is now `192.168.123.1/24`. The firmware version
was advanced to 0.5.2 and the complete host checks pass, including DHCP timeout,
ARP probes and conflict handling for the new address. Program/RAM footprint is
unchanged (62,942 / 3,294 bytes). No programmer operations or live configuration
writes were performed. Saved settings continue to override compiled defaults;
the deployed 0.5.1 device retains `192.168.23.6/24` in configuration sequence 18.
Gateway/DNS defaults are unchanged. See [build metadata](../artifacts/firmware-0.5.2.json).

## Overnight health inspection — 29 October 2026

The deployed 0.5.1 device remained responsive at `192.168.23.13`. Seven status
samples over one minute showed 10 new valid radio readings, 10 captures and
10 MQTT state publications, with no connection-count increase or errors.
HTTP responses took 57–95 ms. Final counters were 7,533 valid readings from
7,539 capture attempts, two MQTT connection attempts (including startup), and
configuration sequence 18. RSSI was about −73 dBm and the transmitter battery
reported OK. DHCP, MQTT, web assets and configuration storage were ready.

Capture attempts include noise triggers, so six rejected captures do not establish
a packet-loss rate. There is no uptime/reset-reason field; the cumulative counts
are consistent with the overnight run but do not prove absence of resets. MQTT
publication counters show local send progress, not broker delivery acknowledgments
(QoS 0). See [raw health samples](../artifacts/health-2026-10-29.json).


## Development 0.6.0 — built, not installed

A permanent PIC bootloader, streaming HTTP update API, bounded package format
and Python uploader are implemented. Two SPI application slots and a redundant
commit-last journal support trial confirmation, previous-image rollback and
three-short-boot recovery without a physical button. The application is linked
at 0x400, below the loader at 0x1cc00; fixed low reset/interrupt vectors remain
outside installer writes. Existing config, EEPROM MAC and web allocations are
preserved. All host checks pass, including the actual boot/update engines under
simulated interrupted writes. Hardware programming, first-boot snapshot and OEM
transition are not yet validated. See [update documentation](firmware-updates.md).

Application footprint: 71,830 program bytes and 3,471 RAM bytes. Bootloader:
5,508 program bytes and 206 RAM bytes (a separate execution phase). Saved build
artifacts and checksums: [0.6.0 metadata](../artifacts/firmware-0.6.0.json).

## 0.6.2 bench progress — 29 October 2026

The corrected permanent loader has passed hardware HTTP update, confirmation,
reset persistence, unconfirmed-trial rollback and three-short-boot recovery tests
with 0.6.0/0.6.1 applications. A table-pointer upper-bank ABI issue discovered on
hardware is fixed; the old 0.6.0 factory artifact is superseded. The exact 0.6.2
factory build also booted and created its confirmed journal during recovery.
Physical power-cut tests remain outstanding.

The local OEM container runs on 192.168.23.30 with wildcard sensornet.info DNS,
SSLv3/RC4-MD5 compatibility and no request allowlist. Real OEM HEXFILE triggering,
metadata exchange and exact SPI staging are verified. A stock-dump self-install
initially failed because its reset vector already pointed to the loader; a
corrected application-entry carrier subsequently rebooted and resumed OEM 3.7.1
reporting. The full clean conversion subsequently succeeded over the network: clean 0.6.2
booted with a confirmed journal, then decoded radio and published to the configured
broker after restoring the private saved settings. Its converter uses 4,062
program bytes and 226 RAM bytes; the full carrier is 229,000 ASCII HEX bytes.
See [OEM transition notes](oem-transition.md) and private bench recovery logs.


## 0.6.3 — complete network round trip

A one-off OEM restore converter and checked two-stage bundle now support custom
HTTP OTA -> scratch upload/readback of the complete stock PIC image -> verified
OEM HEX staging -> restoration of OEM loader/config -> stock installation.
OEM 3.7.1 resumed SSLv3 reporting and radio-data POSTs without PICkit programming.
The stock gateway subsequently converted back to normal custom 0.6.3 through the
LXC server. Configuration, radio, broker-held state/availability and reboot
persistence were verified. The requested fallback 192.168.123.1 is saved.

The explicit public package includes OEM/custom factory, application and
conversion images; source and vendor notices; the Docker server; standalone
upload tools; switching, hardware, build and usage documentation; validation and
SHA256 checksums. Private SPI/EEPROM dumps and credentials are excluded. The
project-wide custom license remains undecided. Physical power cuts remain untested.
