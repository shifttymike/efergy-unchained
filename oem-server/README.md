# OEM firmware update server

This Docker service offers firmware images to the OEM updater. It is not a
telemetry collector. The legacy SSLv3 listener supports unmodified OEM firmware;
the HTTP-only OEM patch uses the backend on port 80.

From the repository root, stage the image you want to offer:

```sh
mkdir -p oem-server/images oem-server/logs
cp binaries/custom/custom-0.6.3-from-oem.hex oem-server/images/firmware.hex
cd oem-server
docker compose up --build -d
```

For the HTTP-only OEM patch, copy
`binaries/oem/oem-3.7.1-http-update.hex` instead. The legacy SSL image fetches
and verifies its OpenSSL source during the Docker build. See
[`../docs/public/oem-server.md`](../docs/public/oem-server.md) and
[`../docs/public/switching.md`](../docs/public/switching.md) for setup and
conversion instructions.
