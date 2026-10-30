# Configuration and operation

## First boot and addressing

The firmware initializes its SPI-flash image slots, configuration and gzipped web
resources automatically. Allow the initial snapshot to finish. DHCP advertises
hostname `efergyhub`. Look up its address in your DHCP server/router and open
`http://GATEWAY_IP/`.

If DHCP has not succeeded after 30 seconds, the gateway probes its saved static
address before using it. The factory default is `192.168.123.1/24`; use a PC address
such as `192.168.123.2/24` on the same LAN for direct configuration. It avoids an
address that responds to its conflict probes. Saved static settings override the
compiled fallback. Changing network settings requires a reboot to apply.

The factory MAC comes from the external EEPROM, which firmware switching leaves
untouched. `/status` reports `mac_fallback: 0` when that read succeeds. A failed
EEPROM read uses emergency MAC `02:ef:67:60:00:01`; repair the EEPROM/connection
rather than operating multiple devices with that fallback address.

## Web UI

The configuration section is collapsed by default. It controls MQTT host, port,
username/password, device name and assumed mains voltage. Username is visible;
only the password is masked. Leaving the password field blank preserves the saved
password. Status polls are serialized, roughly every three seconds, with jitter
and hidden-page suspension. Avoid aggressive polling: there is one HTTP connection.

The graph shows estimated power readings seen since opening the page, capped at
one hour in browser memory. Chart.js is fetched from its CDN; Internet access is
needed for the plotting library. Configuration and status APIs work independently
of the CDN. The UI and its resources are stored gzipped in SPI flash.

## HTTP API

```sh
curl http://GATEWAY_IP/status
curl http://GATEWAY_IP/config
curl http://GATEWAY_IP/update
curl -X POST http://GATEWAY_IP/config \
  --data-urlencode 'host=192.0.2.10' \
  --data-urlencode 'port=1883' \
  --data-urlencode 'username=MQTT_USERNAME' \
  --data-urlencode 'password=MQTT_PASSWORD' \
  --data-urlencode 'device_name=Electricity gateway' \
  --data-urlencode 'voltage_v=230'
curl -X POST http://GATEWAY_IP/reboot -H 'Content-Length: 0'
```

`POST /config` accepts form fields `host`, `port`, `username`, `password`,
`clear_password=1`, `device_name`, `voltage_v`, `dhcp`, `ip`, `mask`, `gateway` and
`dns`. Only supplied fields change. Voltage accepts integer values 1–500 V.
Configuration saves asynchronously; a 202 response starts the save. Check
`save_state`/`save_error` in `/config` or `config_state`/`config_error` in `/status`.
A busy updater, resource writer or configuration save can return 409; retry once
that operation finishes. Passwords are not returned by GET /config.

The HTTP UI/configuration/update APIs and MQTT use the local network without TLS
or HTTP authentication. Keep the device on an appropriate trusted LAN. Neither
CRC firmware checks nor the OEM MD5 mechanism provide a public-key signature.

## MQTT and Home Assistant

Configure a reachable MQTT broker. The gateway uses MQTT 3.1.1 with username and
password support, retained state/discovery and a retained availability Last Will.
Home Assistant's [built-in MQTT integration](https://www.home-assistant.io/integrations/mqtt/) discovers the sensors; no custom HA
integration is required. Topics are based on the gateway MAC and transmitter ID:

* `efergy/<lowercase-MAC>/<transmitter-ID>/state`
* `efergy/<lowercase-MAC>/availability`
* `homeassistant/.../config` discovery records

A state message contains `transmitter_id`, `current_a`, `battery_ok`, `interval`,
`learn`, `power_w`, `voltage_v` and `rssi_dbm`. Up to five transmitter IDs can be
tracked, each with discovery records. The hardware test used one transmitter.
The battery value follows the decoded transmitter status; RSSI describes the
received RF capture. The reporting interval comes from the radio packet.

Estimated power is current multiplied by configured voltage, assuming power
factor 1. It is an estimate of watts, not a direct real-power measurement. The
firmware does not currently integrate energy, retain a historical backlog or
synchronize NTP timestamps. Use Home Assistant's [Integral helper](https://www.home-assistant.io/integrations/integration/) on the power
sensor for best-effort cumulative kWh. MQTT uses QoS 0, so disconnected periods
and missed RF readings are not backfilled.

Yellow pulses indicate radio capture activity, including invalid captures.
Green pulses indicate network RX/TX. `/status` radio-capture and valid-reading
counters are 32-bit boot-relative counters. At a six-second reporting interval,
the valid-reading counter would take about 817 years to wrap. A device reboot
starts both counters from zero; they are not persisted lifetime totals or uptime.

## Requesting the previous custom image

If the alternate slot contains a valid prior custom build, schedule it as a trial:

```sh
curl -X POST http://GATEWAY_IP/update/rollback \
  -H 'Content-Type: application/octet-stream' -H 'Content-Length: 0'
```

Wait until `/update` reports `ready_to_reboot: true`, then POST `/reboot`.
A missing/invalid alternate image is rejected. Trial confirmation checks the
responsive main loop, initialized web resources and network readiness; it does
not independently prove that a particular broker or Home Assistant received data.
