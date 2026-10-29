# Microchip TCP/IP stack 5.42

Microchip-authored source obtained from the historical source mirror:

- Repository: https://github.com/mentatpsi/Microchip
- Commit: `ae5dabf9a9152953ea46f6de11a2b3c185535117`
- Original paths: `Microchip/TCPIP Stack/` and `Microchip/Include/`.
- License headers are retained in each file. The accompanying `Microchip Application Solutions Users Agreement.pdf` is included here. This stack is licensed for use on Microchip devices; it is not relicensed under the project's eventual license.

Only the sources and transitive headers needed by the selected Ethernet, ARP, IP, ICMP, UDP, DHCP, DNS, and TCP modules are included. HTTP and MQTT are application code, with TLS and the large demo applications excluded.

Local compatibility changes for XC8 4.00 C99:

- `include/Compiler.h` in the project replaces the original compiler-selection header through include-path precedence and selects the PICC18 implementations, with current `xc.h` device definitions.
- `GenericTypeDefs.h`: use XC8's `unsigned __int24` spelling for `UINT24`.
- `ETH97J60.c`: use `__at()` for the original Ethernet errata scratch locations; signal an application activity flag on validated Ethernet reception and transmit launch for the green status LED.
- `DHCP.c`: advertise the configured fixed hostname as DHCP option 12 in outgoing packets, in addition to requesting the hostname parameter.
- `DNS.c`, `TCP.c`, `UDP.c`: spell the legacy persistent declarations using XC8's prefix `__persistent` qualifier.
- `ARP.c`: require IPv4 ARP and notify the application of received claims for fallback-address conflict checking.
- `TCP.c`: expose a small read-only TCP-phase diagnostic hook.
- `Helpers.c`: require four nonempty octets before treating a host string as IPv4, avoiding partially initialized addresses for short numeric names.
- `DNS.c`: validate response flags/question count and accept A records from the answer section only; referrals and unrelated authority/additional addresses are not broker addresses. The MQTT application owns DNS lookup explicitly before opening TCP by IP.

The remaining vendor warnings about conversions/unused routines are visible in build output. A successful target build does not establish network or silicon-errata behavior on this board; that is a bring-up gate.
