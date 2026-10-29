# OEM firmware update server

This Docker service offers firmware images to gateways running OEM 3.7.1. Use it
to install custom firmware or the HTTP-only OEM patch without PICkit.

It is **not** a telemetry collector or replacement Efergy service. The bundled
SSLv3 listener exists only so the unmodified OEM updater can reach the update
service. Use an existing community solution for local telemetry collection.

## Choose the image

From the release root, copy one image into `oem-server/images/firmware.hex`:

```sh
cp firmware/custom/custom-0.6.3-from-oem.hex oem-server/images/firmware.hex
```

Or, to bypass SSL in the OEM application:

```sh
cp firmware/oem/oem-3.7.1-http-update.hex oem-server/images/firmware.hex
```

Power off the gateway before starting the server. If `images/firmware.hex`
exists, the backend offers it automatically to eligible OEM gateways:

```sh
cd oem-server
mkdir -p images logs
docker compose up --build -d
docker compose logs --tail=50
```

The legacy SSL image fetches and checksum-verifies OpenSSL 1.0.2u during this
build. No separate source-fetch command is needed.

Point LAN DNS for `*.sensornet.info` to the Docker host and allow TCP 80 and 443
from the gateway. Keep `uk.pool.ntp.org` on normal DNS for the HTTP-only patch.
Wait for `curl --fail http://127.0.0.1/health` to return `OK`, then power on the
gateway. Keep stable external power while the OEM loader installs the image.

After the gateway reboots, verify installation at `http://GATEWAY_IP/status`;
for the custom image, confirm `firmware` is `0.6.3`, `network_state` is `1`, and
`mqtt_state` is `4`. A request for `/firmware.hex` and its delivery marker only
confirm transfer. Stop the server with `docker compose down` after verification.

Each device/image pair is offered once. To retry after an incomplete update,
power off the gateway, then from `oem-server/` clear its marker before powering
it on again:

```sh
MAC=001122AABBCC
IMAGE_ID=$(python3 -c 'import hashlib,pathlib; print(hashlib.sha256(pathlib.Path("images/firmware.hex").read_bytes()).hexdigest()[:16])')
rm -f "logs/delivered-${MAC}-${IMAGE_ID}.json"
```

Use the gateway MAC in uppercase without separators. See
[`docs/switching.md`](../docs/switching.md) for the complete conversion procedure.
