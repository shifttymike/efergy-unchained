# Third-party material and notices

The project-authored custom firmware, flash tools and OEM server code are GPL-3.0-only;
see the root `LICENSE` and `LICENSE-SCOPE.md`. That grant does not replace the
licenses or terms below.

## Microchip TCP/IP stack

The source subset in `source/vendor/microchip/` is Microchip TCP/IP Stack 5.42,
with provenance and modifications documented in
`source/vendor/microchip/PROVENANCE.md`. Original file notices remain in place.
The accompanying `Microchip Application Solutions Users Agreement.pdf` grants
use with Microchip products and allows distribution only if recipients agree to
be bound by the agreement, which must accompany distribution. It is not GPL code.
Confirm the terms for public source and firmware distribution with Microchip or
qualified counsel before publishing the bundled source/binary.

## OEM firmware

`firmware/oem/` contains OEM firmware 3.7.1 supplied for restoration and tested
conversion. It remains separate OEM copyrighted material. It is not under this
project's GPL license. Obtain redistribution permission before publishing these
images in a public repository.

## Other dependencies

* The OEM server build fetches OpenSSL 1.0.2u from upstream using the pinned
  SHA-256 in `oem-server/legacy-ssl/get-sources.sh`; the source archive is not committed.
  The OpenSSL license is included in the fetched source. Check its terms for the
  server's static link/build arrangement before redistributing a built image.
* The device UI loads Chart.js from a CDN at runtime. Chart.js is not bundled in
  this release; its upstream license applies to the downloaded script.
* The radio implementation was independently cross-checked against the rtl_433
  Efergy e2 classic decoder. No rtl_433 source is included in this release.

Preserve all original copyright and license notices when copying any third-party
component. See `LICENSE-SCOPE.md` for the boundary of the project's GPL grant.
