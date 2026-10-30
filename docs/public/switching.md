# Flashing and recovery

Use a stable external supply for every conversion. Keep PICkit target power
disabled. The one-time OEM/custom loader conversions have a commit phase that is
not power-fail safe.

## OEM 3.7.1 to custom

1. Power off the gateway. From the release root, stage the OEM conversion image:

   ```sh
   mkdir -p oem-server/images oem-server/logs
   cp firmware/custom/custom-0.6.3-from-oem.hex oem-server/images/firmware.hex
   ```

2. Point LAN DNS for `*.sensornet.info` to the Docker host. Allow TCP 80 and 443
   from the gateway. Keep the gateway powered off while starting the server.
3. Start the server and check that its health endpoint responds:

   ```sh
   cd oem-server
   docker compose up --build -d
   curl --fail http://127.0.0.1/health
   ```

4. Power on the gateway and leave its external supply connected. Follow the
   backend logs until it requests `/firmware.hex`, then allow several minutes for
   conversion and reboot:

   ```sh
   docker compose logs -f backend
   ```

5. Find the gateway's DHCP lease (`efergyhub`) and open `http://GATEWAY_IP/status`.
   Confirm `firmware` is `0.6.3`, `network_state` is `1`, and `mqtt_state` is `4`.
   A `/firmware.hex` request alone does not confirm installation.
6. Stop the server after the gateway finishes:

   ```sh
   docker compose down
   ```

The Docker service distributes OEM-format images; it is not a telemetry receiver.

### Retry after an incomplete update

After serving `/firmware.hex`, the backend holds an in-memory lock for that
gateway and image. This prevents repeated offers while the backend container is
running. The lock clears automatically when that container restarts; no marker
files are written. If conversion did not complete and the gateway still runs OEM
firmware, wait until it has settled, then restart the backend. The gateway does
not need to be restarted:

```sh
docker compose restart backend
```

The image will be offered on the gateway's next check-in. Do not restart the
backend during a firmware transfer or while conversion code is writing flash.
After the gateway reboots, verify `/status` reports custom firmware before
considering the update complete.

## OEM 3.7.1 to HTTP-only OEM

This installs the OEM application patch, not custom firmware. It bypasses the
SSL client path and changes affected service requests to HTTP port 80. It retains
`uk.pool.ntp.org` and the OEM `h3`, `keys` and `update` hostnames. The patch has
not been flashed or validated on hardware.

1. Stage the update image instead of the custom image:

   ```sh
   mkdir -p oem-server/images oem-server/logs
   cp firmware/oem/oem-3.7.1-http-update.hex oem-server/images/firmware.hex
   ```

2. Keep DNS for `*.sensornet.info` pointed at the Docker host. Leave
   `uk.pool.ntp.org` on normal DNS. Allow TCP 80 and 443: stock firmware uses
   SSLv3 on port 443 to request and download its update; the patched app uses
   HTTP on port 80 after reboot.
3. From `oem-server/`, build and offer the image:

   ```sh
   docker compose up --build -d
   docker compose logs -f
   ```

4. Keep power connected until the OEM loader finishes. The app still reports
   version 3.7.1; compare the delivered hash with
   `firmware/oem/oem-3.7.1-http-patch.json` to identify the patch.
5. Stop the server after the gateway resumes reporting:

   ```sh
   docker compose down
   ```

## Custom to newer custom

```sh
python3 tools/upload_firmware.py http://GATEWAY_IP \
  firmware/custom/custom-0.6.3-update.efu --reboot
```

The loader trials the image and rolls back if it fails to confirm. Check
`/update` until the operation is complete.

## Custom to OEM 3.7.1

```sh
python3 tools/restore_oem.py http://GATEWAY_IP \
  firmware/oem/oem-3.7.1-from-custom.zip --commit
```

The tool installs a temporary converter, uploads and verifies the OEM image, then
restores the OEM loader and application. Allow several minutes; do not interrupt
power during commit. OEM firmware may erase custom SPI configuration. Save MQTT
settings first. To stage without committing, omit `--commit`; later run the same
command with `--commit-only`.

## PICkit programming

Use the full `*-pickit.hex` for the firmware you want. Select PIC18F67J60 in
MPLAB IPE, program and verify all PIC memory, then release reset. The board needs
external power and ICSP VDD sense/ground; leave programmer power disabled. A
PICkit readback does not back up the external EEPROM or SPI flash.
