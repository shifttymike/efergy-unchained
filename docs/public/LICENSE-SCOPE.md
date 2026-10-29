# License scope

## Project-authored code

The root `LICENSE` applies to project-authored code in `source/`, `tools/`, and
`oem-server/`, including the custom application, permanent bootloader,
OEM-to-custom bootstrap, custom-to-OEM converter, host-side packaging/upload
tools, and OEM server implementation. It is **GNU GPL version 3 only** (SPDX
`GPL-3.0-only`). The release includes the complete license text.

The license does not apply to a file merely because it is in this repository or
because it is needed to build or use the firmware. File-specific copyright and
license notices take precedence for their respective material.

## Excluded or separately licensed material

* `source/vendor/microchip/` is Microchip-authored TCP/IP stack code and headers.
  Its original notices and `Microchip Application Solutions Users Agreement.pdf`
  are retained. It is not relicensed under GPL. That agreement restricts use to
  products incorporating Microchip products and conditions redistribution on the
  recipient agreeing to the agreement. A public GitHub release should not be
  treated as satisfying that click-through/acceptance condition without separate
  confirmation from Microchip or qualified counsel.
* `firmware/oem/` contains vendor OEM firmware 3.7.1 and a converted restoration
  package. These remain OEM copyrighted works. The GPL grant for project code
  does not grant rights to redistribute these images; confirm permission before
  public distribution. The user's local OEM dump was used to prepare the package.
* OpenSSL is fetched by the OEM server build script from its upstream source and
  is not included as project-authored code. Its license and notices apply to the
  fetched library. Because the legacy proxy links against this OpenSSL version,
  review license compatibility before redistributing a built container image.
  The release contains source and a Dockerfile, not a built container image.
  Chart.js is fetched from its CDN at runtime and is not bundled.
* Other third-party code and documentation remain subject to the notices shipped
  with them. See `THIRD-PARTY-NOTICES.md`.

## Firmware image scope

The custom PIC images combine project-authored code with the Microchip TCP/IP
stack. The GPL license in this release is a grant for the project-authored parts;
it does not override the Microchip agreement. Before representing the complete
linked image as distributable under GPL terms, resolve the compatibility and
redistribution requirements for the Microchip components. GPLv3 installation
information for the supported firmware update/recovery process is documented in
`docs/switching.md` and `docs/building.md`.

OEM images are provided as separate restoration/conversion artifacts, not under
the project GPL grant.
