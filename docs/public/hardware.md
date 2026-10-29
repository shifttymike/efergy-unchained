# Hardware and recovery

Validated board: EH433M3v1, PIC18F67J60 (128 KiB program flash, about 3.8 KiB RAM),
RFM69W radio, SST25VF040B 512 KiB SPI flash and an external MAC EEPROM. The PIC
contains the Ethernet MAC/PHY and packet RAM; TCP/IP, HTTP and MQTT run in firmware.
No UART or physical pairing/reset button is assumed by the custom firmware.

The stock PICkit-compatible ICSP header programs the PIC. Use the board's normal
external supply and connect ICSP ground, VDD sense, reset/VPP, PGC and PGD in the
correct orientation. Never enable programmer-supplied target power. A PICkit
readback is not a backup of the external EEPROM or SPI chip.

## Custom memory layout

| Region | Address |
| --- | --- |
| Permanent PIC reset/interrupt vectors | `0x00000–0x003ff` |
| Custom PIC application | `0x00400–0x1cbff` |
| Permanent PIC loader/configuration | `0x1cc00–0x1ffff` |
| SPI application slot 0 | `0x00000–0x1cfff` |
| SPI application slot 1 | `0x1d000–0x39fff` |
| SPI update journals | `0x3a000–0x3bfff` |
| SPI configuration sectors | `0x4f000`, `0x50000` |
| SPI web resource sectors | `0x51000–0x52fff` |
| Temporary OEM PIC-image upload | `0x60000–0x7ffff` |

The OEM converter's final commit repurposes the lower SPI area for the OEM
loader's HEX staging file. OEM firmware uses its own storage format and may erase
custom configuration/resources. The MAC EEPROM is not rewritten.

## Recovery behavior

Ordinary custom OTA verifies the complete staged image before committing a
pending journal record. The permanent loader verifies and installs it, then marks
it as a trial. A reset before confirmation restores the previous active slot.
After three short boots a valid alternate image can be selected automatically.
CRC-protected records are committed last and rotated across two journal sectors.

Use the hardware tests in the release validation notes to distinguish tested
recovery behavior from modeled power-loss behavior. Simultaneous corruption of
both slots/journals or interruption during a one-time bootloader conversion can
require PICkit. Restore a known full PIC image and verify it. If IPE holds reset,
release it after programming. Check external power and ICSP VDD sense first when
PICkit reports a connection or target-voltage failure.
