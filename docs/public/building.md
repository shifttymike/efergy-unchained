# Source and build

The source archive contains the custom application, permanent loader, OEM-to-custom
bootstrap, one-off OEM return converter, portable host tests and packaging tools.
Build outputs, compiler installations and private bench material are excluded.
Project-authored firmware and support tools are GPL-3.0-only within the scope in
`LICENSE-SCOPE.md`. Vendor notices and restrictions remain separate; read them
before redistributing source or images.

Use Microchip XC8 4.00 and PIC18F-J DFP 1.7.159 for the recorded builds. Host tests
use Clang, Python 3 and Node.js. Compiler/DFP installation is separate from the
public source. From `source/`:

```sh
make XC8=/path/to/xc8-cc DFP=/path/to/PIC18F-J_DFP/1.7.159/xc8
make check
make oem-transition XC8=/path/to/xc8-cc DFP=/path/to/dfp/xc8
make oem-restore OEM_HEX=../firmware/oem/oem-3.7.1-pickit.hex \
  XC8=/path/to/xc8-cc DFP=/path/to/dfp/xc8
make oem-restore-check
```

Normal artifacts are `build/efergy-factory.hex` (PICkit),
`build/efergy-update.efu` (custom HTTP OTA), and
`build/transition/efergy-oem-transition.hex` (OEM update server).
The OEM return bundle is `build/oem-restore/oem-3.7.1-network-restore.zip`.
The converter build is separate; it does not replace the normal application.
The OEM packager checks vector layout, address bounds, staged file length,
model and CRCs. The OEM version/header constants are generated from the supplied
stock image, not from the device's EEPROM or its stored MQTT configuration.

The recorded compiler flags are `-mcpu=18F67J60 -O2 -std=c99`, application code
at `0x400` with explicit ROM bounds, and permanent loader code at `0x1cc00`.
The source includes the legacy Microchip TCP/IP subset and its accompanying
license. Source and release packaging preserve those notices.

`tools/build_release.py` assembles the explicit public file list and checksums.
It expects the normal build, OEM transition and OEM restore artifacts to exist.
Pass the original OEM HEX using `--oem`; do not substitute an EEPROM or SPI dump.
The public image binaries are already built, so most users do not need XC8. To
reproduce the release, use the listed compiler/DFP versions and source tree. The
normal custom build and one-off converter each produce their own `.efu` update
package; factory HEX packaging and OEM conversion package generation are separate
steps in the make targets.
