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

## Optional DHCP and DNS

The `lan` Compose profile starts dnsmasq on the host network. It offers DHCP
leases and resolves `sensornet.info` and its subdomains to the update server.
Use this only on an isolated lab network/VLAN or after disabling the existing
DHCP server on that network. Two DHCP servers on one LAN can issue conflicting
addresses and gateways.

Copy `oem-server/.env.example` to `oem-server/.env` and set the host interface,
update-server IP, DHCP range, subnet mask, router and upstream DNS for that LAN.
Then start the stack with:

```sh
docker compose --profile lan up --build -d
```

Without `--profile lan`, DHCP and DNS are not started. On Linux, ensure the host
interface and ports 53/67 are available to dnsmasq. Docker Desktop host networking
does not provide the LAN broadcast access needed for DHCP.

After the gateway reboots, verify installation at `http://GATEWAY_IP/status`;
for the custom image, confirm `firmware` is `0.6.3`, `network_state` is `1`, and
`mqtt_state` is `4`. A request for `/firmware.hex` only confirms transfer. Stop
the server with `docker compose down` after verification.

After serving an image, the backend holds an in-memory lock per device and image
until the backend container restarts. No delivery marker files are written. To
retry after an incomplete update, wait until the gateway has settled, then restart
the backend; the hub does not need to be restarted:

```sh
docker compose restart backend
```

The image is offered again on its next check-in. Do not restart the backend
during a firmware transfer or while conversion code is writing flash. See
[`docs/switching.md`](../docs/switching.md) for the complete conversion procedure.
