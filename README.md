# Efergy local MQTT firmware

Custom firmware for the EH433M3v1 gateway: PIC18F67J60, RFM69W and SST25VF040B.
Version 0.6.3 preserves e2 classic radio decoding, publishes current/power/RSSI/
battery/interval/voltage through MQTT and Home Assistant discovery, and provides
persistent configuration, a small SPI-hosted web UI and recoverable HTTP OTA.

* [Configuration, HTTP API, MQTT and UI](docs/public/usage.md)
* [Switching OEM/custom over the network or using PICkit](docs/public/switching.md)
* [Hardware and memory layout](docs/public/hardware.md)
* [Build and source](docs/public/building.md)
* [Validation and limitations](docs/public/validation.md)
* [Licensing and third-party notices](docs/public/THIRD-PARTY-NOTICES.md)

```sh
make
make check
make oem-transition
make oem-restore OEM_HEX=/path/to/original-stock.hex
make oem-restore-check
```

Recorded toolchain: XC8 4.00, PIC18F-J DFP 1.7.159; override `XC8`/`DFP` if needed.
The normal factory image is `build/efergy-factory.hex`, the custom update package
is `build/efergy-update.efu`, and the OEM conversion carrier is
`build/transition/efergy-oem-transition.hex`. `tools/build_release.py` creates the
public distribution using an explicit file list and checksums; it excludes bench
recovery files, SPI/EEPROM dumps, credentials and local build/runtime state.

Use externally powered hardware with PICkit target power disabled. One-time
bootloader conversions require uninterrupted power. Project-authored custom code
is GPL-3.0-only; third-party and OEM material keeps separate terms described in
the public release's `LICENSE-SCOPE.md`. Detailed internal bench
history remains under `docs/` and `artifacts/`, outside the public release list.
