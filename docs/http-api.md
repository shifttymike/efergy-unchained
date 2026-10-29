# Firmware 0.5.2 HTTP API

The current bench device uses DHCP at `192.168.23.13`, advertising hostname
`efergyhub` (DHCP option 12, including discovery, requests and renewals). This
network hostname is fixed and separate from the configurable Home Assistant
device name; DNS registration depends on the DHCP/DNS server. Its saved static fallback
is `192.168.23.6/24`, gateway/DNS `192.168.23.254`. Firmware 0.5.2 changes
the compiled default IP to `192.168.123.1/24`; saved configuration takes
precedence and is not migrated. Gateway/DNS defaults are unchanged. HTTP port is 80. The firmware
serves the self-contained browser UI at `GET /` (gzip HTML/JS), plus JSON API
endpoints at `/status` and `/config`. The UI polls status about every three seconds while visible. HTTP and MQTT are plaintext on the LAN.

## Configure MQTT

```sh
curl --fail-with-body http://192.168.23.13/config \
  --data-urlencode 'host=YOUR_BROKER' \
  --data-urlencode 'port=1883' \
  --data-urlencode 'username=YOUR_USERNAME' \
  --data-urlencode 'password=YOUR_PASSWORD'
```

Hostnames use the configured unicast DNS server; mDNS (`.local`) is not implemented.

Only supplied fields change. Use `--data-urlencode` so spaces, `+`, `&`, `%`,
and `=` in credentials are encoded correctly. Supported values:

| Field | Meaning |
|---|---|
| `host` | DNS hostname or IPv4 address, up to 63 ASCII characters; empty disables MQTT |
| `port` | TCP port, 1–65535; default 1883 |
| `username` | Up to 31 printable ASCII characters; empty clears it |
| `password` | Up to 63 printable ASCII characters; **empty preserves the existing password** |
| `clear_password=1` | Explicitly clear the password, regardless of field order |
| `device_name` | Friendly device name, 1–31 printable ASCII characters, not all spaces; default `Efergy gateway` |
| `voltage_v` | Assumed mains voltage in whole volts, 1–500; default 230; applies after commit |
| `dhcp` | `1` or `0`; applies after reboot |
| `ip`, `mask`, `gateway`, `dns` | Dotted IPv4 network settings; apply after reboot |

A changed valid configuration returns HTTP **202** with `saving` and the next
sequence number. This acknowledges the queued operation, not a completed flash
commit. Poll `GET /config` until `save_state` is zero and `sequence` reaches that
number. Nonzero `save_error` means the update failed and the previous committed
configuration remains active. An identical patch returns HTTP **200** with
`{"unchanged":true}` and does not erase/program flash.

```sh
curl --fail http://192.168.23.13/config
curl --fail http://192.168.23.13/status
```

Reads return the saved `username` and the `username_set` / `password_set` flags.
The password value is never returned.
MQTT settings, device name and assumed voltage apply after commit, without rebooting. A voltage
change reconnects MQTT, republishes discovery, and recalculates the latest cached
reading. Existing broker credentials are preserved when omitted. Name changes update the
Home Assistant device name; stable MAC/transmitter identifiers and topics remain
unchanged. Device names are escaped in JSON. The UI leaves saved credentials
blank and omits those fields unless replaced or explicitly cleared. Network settings are staged
until reboot; `/status` reports `network_pending:1` when they differ from the
active settings.

To disable MQTT and clear temporary credentials:

```sh
curl --fail-with-body http://192.168.23.13/config \
  --data-urlencode 'host=' --data-urlencode 'username=' \
  --data-urlencode 'clear_password=1'
```

## Device name and browser UI

Open `http://192.168.23.13/`. Configuration includes device name, broker, port,
username, password and assumed voltage. Configuration is collapsed by default;
select its heading to expand it. The saved username is visible. Blank credential inputs preserve saved
values; separate checkboxes explicitly clear them. Saving pauses status polling,
waits for the asynchronous flash commit, and then resumes polling. Each poll is scheduled after the previous request
completes, with up to 0.5 s jitter to separate viewers. Failed polls back off to
6, 12 and 15 seconds; hidden pages stop polling and refresh when shown again.
Reload already-open pages after firmware updates to load the new polling code. Form values
are not overwritten by status updates while being edited.

```sh
curl --fail-with-body http://192.168.23.13/config \
  --data-urlencode 'device_name=House meter'
```

Long polling is not used: it would occupy the single HTTP connection while
waiting and prevent another viewer or configuration request from being served.

The browser displays amps, estimated watts, RSSI, reporting interval, battery,
assumed voltage, MQTT state, network address, transmitter ID and capture counts.
Raw status JSON is available under Device diagnostics. Unknown or locally stale
radio readings are shown as unavailable. Requests do not overlap; no external
scripts, fonts or services are needed for status/configuration. The optional power
chart loads pinned Chart.js 4.5.1 directly from jsDelivr over HTTPS, with SRI
integrity checking; the library is not stored on the PIC or SPI flash.

The page is a 16-byte committed WEB2 header followed by gzip data in SPI flash
at `0x51000–0x52fff` (8 KiB). On boot, the firmware compares the installed image
with its compiled installer copy. An unchanged page is not rewritten. Otherwise
it erases only these sectors, programs incrementally, verifies every byte, then
writes the commit marker last. A reset during installation is repaired on boot.
Root returns 503 until the verified page is ready; `/status` remains available.
Configuration writes/reboot return 409 while assets are being installed.
`web_state` is 0 ready, 1 scan, 2 erase, 3 program, 4 verify, 5 commit, 255 failed.

The PIC streams 256-byte chunks with `Content-Type: text/html` and
`Content-Encoding: gzip`; the browser handles decompression. Use
`curl --compressed http://192.168.23.13/` when inspecting HTML from a terminal.
To update the page, rebuild/flash the firmware; arbitrary HTTP flash upload is
not exposed.

## Browser power history

The Power history panel plots estimated watts from newly observed readings.
It stores points only in the current page's memory, capped to a rolling hour.
Reloading or closing the page clears the history. There is one point per new
valid-reading counter, even when the wattage is unchanged; duplicate status polls
do not add points. Each point keeps its voltage assumption at the time received.
Counter jumps/resets, disconnected/hidden periods and long receive gaps break
the plotted line instead of fabricating readings. Points use browser receive
times, not device/NTP timestamps. Existing 3-second polling is unchanged and
plotting makes no additional requests to the PIC.

The graph uses a numeric time axis, so no date-adapter library is needed. It
loads asynchronously; status, settings and point collection continue if the CDN
is slow or unavailable. Readings missed while hidden/disconnected cannot be
recovered: there is no device-side history endpoint. Browser memory also has a
2,400-entry guard including gap markers, above normal one-hour radio traffic.

## Assumed voltage and estimated power

```sh
curl --fail-with-body http://192.168.23.13/config \
  --data-urlencode 'voltage_v=240'
# Poll until sequence reaches the returned next_sequence and save_state is 0.
curl --fail http://192.168.23.13/config
```

`voltage_v` is exposed in configuration JSON, MQTT state and the Home Assistant
**Assumed voltage** diagnostic sensor. It is a configured assumption, not a
voltage measurement. **Estimated power** uses the raw current mantissa:
`power_w = current_a × voltage_v`, rounded to 0.1 W, assuming power factor 1.
Reactive/nonlinear loads and actual voltage variation make this an estimate.
The calculation uses the unrounded current; multiplying the displayed three-
decimal amps may differ slightly from the published power.

Firmware 0.4 reads deployed version-1 flash records without changing them and
uses 230 V for those records. Version-2 records retain voltage; both older
formats default the new device name to `Efergy gateway`. The next successful POST saves a version-3,
231-byte record in the inactive slot, retaining omitted settings/credentials.
Older firmware cannot read version-3 records; rollback should not be expected
to preserve the latest settings once both slots have migrated.

## Reboot or set a static address

```sh
curl --fail-with-body http://192.168.23.13/config \
  --data-urlencode 'dhcp=0' --data-urlencode 'ip=192.168.23.6'
# Wait for the flash commit before rebooting.
curl --fail-with-body -X POST --data '' http://192.168.23.13/reboot
# After reboot, use http://192.168.23.6/status instead.
```

Static mode and DHCP failure both probe the saved IP with ARP before claiming
it. A conflicting claim withdraws the address; the firmware retries later rather
than using the duplicate IP. The DHCP timeout is 30 seconds after link becomes
available. Static probing takes about four seconds. Reboot is refused with 409
while a configuration write is in progress.

## MQTT and Home Assistant

The broker must accept MQTT 3.1.1 without TLS. Client ID is
`efergy-<12 lowercase MAC hex digits>`. This board uses:

| Topic | Retained payload |
|---|---|
| `efergy/5410eca5a92a/availability` | `online`; retained Last Will is `offline` |
| `efergy/5410eca5a92a/827781/state` | JSON with transmitter ID, current in amps, battery status, interval and learn mode |
| `homeassistant/sensor/efergy_5410eca5a92a_827781_current/config` | Current sensor discovery |
| `homeassistant/binary_sensor/efergy_5410eca5a92a_827781_battery/config` | Battery discovery |
| `homeassistant/sensor/efergy_5410eca5a92a_827781_rssi/config` | Signal strength diagnostic discovery, dBm |
| `homeassistant/sensor/efergy_5410eca5a92a_827781_power/config` | Estimated power discovery, W |
| `homeassistant/sensor/efergy_5410eca5a92a_827781_voltage/config` | Assumed voltage diagnostic discovery, V |
| `homeassistant/sensor/efergy_5410eca5a92a_827781_interval/config` | Reporting interval diagnostic discovery, s |

Each observed transmitter gets its own stable device/entity identifiers. Five
transmitters are cached; broker outages retain the latest reading per transmitter
in RAM. This is not a historical offline outbox. Discovery and latest state are
republished after MQTT reconnect. Retained discovery supports a fresh HA
subscription without a separate birth-message subscription. Sensor expiration
is five reporting intervals, with a minimum of 30 seconds, so a missing radio
transmitter is distinct from the gateway's MQTT availability.

Keepalive is 30 seconds, with PINGREQ after 15 seconds without other outgoing
packets and a 10-second PINGRESP deadline. Reconnect backoff is capped at 60
seconds. Publishing uses QoS 0. State JSON includes `current_a`, `power_w`,
`voltage_v` and `rssi_dbm` as well as the existing transmitter/battery fields.
Accumulated energy is not implemented.

RSSI is the receiver's register value sampled at the start of the receive
interrupt, before timed capture can continue into post-transmission noise.
The RFM69W mapping is `-raw / 2` dBm, with 0.5 dB resolution. It is the gateway's
received RF signal level, not an upstream network signal or a calibrated meter.
The raw uninitialized register value 255 is published as JSON `null`. Only
checksum-valid readings update the transmitter's signal sensor.

## Diagnostics

`GET /status` returns active IP/mode, radio counters, latest raw current
mantissa/shift, MQTT status and configuration-save status. `radio_version`, `capture_profile`,
`capture_bits` and `capture_raw` expose the latest capture for RF diagnosis;
`capture_raw` contains its first eight bytes, before inversion/checksum checking.
`rssi_raw` is the latest valid reading's RSSI register byte (255 before one exists);
`voltage_v` is the configured voltage assumption. Amps can be computed
as `current_mantissa / 2^current_shift`. MQTT JSON already contains scaled amps.

| Status | Values |
|---|---|
| `mqtt_state` | 0 disabled/backoff, 1 TCP connecting, 2 send CONNECT, 3 await CONNACK, 4 connected, 5 DNS lookup |
| `mqtt_error` | 0 none, 1 socket unavailable, 2 connection lost, 3 TCP timeout, 4 malformed MQTT, 5 CONNACK timeout, 6 PING timeout, 7 DNS failure, 8 packet too large/invalid, 9 short TCP write |
| `mqtt_error` 21–25 | MQTT CONNACK refusal: protocol, identifier, server unavailable, credentials, authorization |
| `mqtt_tcp_phase` | Legacy stack TCP state in low 7 bits; high bit indicates an enabled TCP timer |
| `network_state` | 0 DHCP wait, 1 DHCP bound, 2 probing static IP, 3 static active, 4 address conflict |
| `config_state` / `save_state` | 0 idle, 1 erase/program wait, 2 readback verification, 3 commit wait, 255 failed |

The final build returns 404 for `GET /flash.bin`. Raw flash download is a bench
backup feature enabled only in an explicitly instrumented build; it can expose
the plaintext credential records and is disabled for normal use.

Request bodies are bounded to 767 bytes, individual header lines to 511 bytes,
and total headers to 2,048 bytes. Requests need Content-Length; chunked input,
duplicate lengths, unsupported methods and malformed form fields are rejected.
Only one HTTP connection is served at a time, alongside the MQTT connection.
POST errors do not echo supplied values.

## Development 0.6.0 update API

The unflashed development build adds `/update` staging/status, binary chunk
upload, commit, cancel and rollback endpoints. It requires the new permanent
bootloader; these endpoints do not exist in deployed firmware 0.5.1. See
[firmware update protocol and recovery](firmware-updates.md).
