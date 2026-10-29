# Development board and bench

- PIC18F67J60, RFM69W, SST25VF040B, 24AA02E48.
- EUI-48: `54:10:EC:A5:A9:2A`; RouterOS DHCP/ARP baseline: `192.168.23.13`.
- LAN: `192.168.23.0/24`, gateway/DNS `192.168.23.254`.
- DHCP pool: `192.168.23.10–192.168.23.253`.
- Default fallback: `192.168.23.6/24`, outside the DHCP pool. Static setup was exercised on hardware; the firmware probes before claiming the address and withdraws on conflict. Conflict/lease-loss behavior also has host tests.
- Router access is for read-only observation. Login credentials are not stored in this project.
- PICkit 3 connected; **external target power only**. Never pass IPECMD `-W`, which enables programmer power.
- No UART. No logic analyzer attached. Diagnostic HTTP is the first observation path after network bring-up.
- Leave clean firmware installed between development passes; no automatic restoration to the original image. Keep permanent external power during programming.
- A permanent external supply replaced the powerbank on 2026-10-28. The powerbank slept during the first programming attempt because target current fell during programming. The resulting verify error and missing VDD were followed by a restore attempt; see bring-up notes for the verified final state.

## Pin evidence

| Function | PIC pins | Original code evidence |
|---|---|---|
| RFM69 software SPI | RE5 CS, RE3 SCK, RE4 MOSI, RD0 MISO | `0x00f572`, `0x00f6ec` |
| RFM69 reset | RE1 | initialization `0x00c0fe–0x00c110` |
| Continuous receive data | RB2 | timed sampler `0x00ad7a–0x00aedc` |
| Continuous receive interrupt | RB port change | `0x00f536–0x00f548`; exact DIO-to-RB wiring still needs corroboration |
| Packet receive interrupt | INT2/RB2 | `0x00f520–0x00f52e` |
| SPI flash | RC1 CS, RC3 SCK, RC4 MISO, RC5 MOSI | CS brackets SSP1BUF command/address transfers at `0x017362–0x0173d2` |
| EEPROM I2C | RA4 SDA open drain, RA5 SCL | `0x019eac–0x019fd8`; EUI-48 at `0xfa` selected by `0x019d88` |

The assembly receive loop uses RAM `0x0e00–0x0e17`; the adapted Ethernet driver reserves `0x0e7e–0x0e7f` for its documented silicon-errata workarounds. The compiler knows about both reservations.


## Activity LEDs

The clean image pulses the two GPIO status LEDs for approximately 125 ms:
RF3 for radio captures (yellow), RD1 for Ethernet RX/TX (green). Repeated events
extend the pulse; sustained traffic can hold an LED on. Radio activity includes
captures that fail decoding/checksum, so yellow does not imply a valid reading.
Green indicates packet activity, including DHCP/ARP/HTTP, not broker acceptance.

Original firmware configures RD1 and RF3 as outputs and clears both latches at
`0x00c112–0x00c118`; later status code sets/clears/toggles them. The user visually confirmed the color assignment
and active-high flashing on the physical board after programming.
The Ethernet jack's PHY-controlled link/activity LEDs remain separate.
LED timing runs in the main loop; no delays or changes to the radio sampler are
introduced. Sampler instruction-path audit remains 77 cycles.


## Flash-hosted UI

Firmware 0.4 owns asset sectors `0x51000` and `0x52000` in addition to the two
configuration sectors. They were erased in the preserved full pre-config SPI
backup, and earlier flash isolation readback confirmed those bytes unchanged.
The asset HAL admits only these two sectors and bounded writes within 8 KiB.
The page's gzip bytes, committed header and generated installer image are
reproducible with `make assets`; the browser does the decompression. The PIC
streams the image and continues radio/MQTT work between chunks.


Firmware 0.4.2 sends the fixed hostname `efergyhub` as DHCP option 12 in
DISCOVER, REQUEST and renewal messages. It is independent of the UI/HA device
name. The unit's EEPROM MAC and saved network/MQTT settings remain unchanged.
