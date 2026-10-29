#!/usr/bin/env python3
"""Build OEM 3.7.1 HTTP-only application images from the untouched OEM HEX.

The fixed patch bypasses the application's SSL client-start branch and changes
only the affected service ports from 443 to 80. It deliberately accepts only
the canonical EH433M3v1 OEM 3.7.1 HEX so endpoint/country-modified dumps cannot be
mistaken for the stock base. The generated full PIC HEX is for PICkit; the
application HEX is for the OEM Docker update server.
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from package_firmware import read_hex, write_hex  # noqa: E402
from package_oem_restore import stock_images  # noqa: E402

OEM_FILE_SHA256 = "613143553c43947337ac3a9ae3f9ebd40b1abccb53741bf73793646a9d55f6a0"
OEM_LOADED_SHA256 = "be74b54132736a5e7a8a1a90b5034ac0cd4b2a51650a042bc5f45862efdd0584"
PATCHES = (
    (0x557A, bytes.fromhex("05e1"), bytes.fromhex("0000"), "bypass TCPStartSSLClient branch"),
    (0x5362, bytes.fromhex("bb0e"), bytes.fromhex("500e"), "keys service port 443 to 80, low byte"),
    (0x5366, bytes.fromhex("010e"), bytes.fromhex("000e"), "keys service port 443 to 80, high byte"),
    (0x5522, bytes.fromhex("bb0e"), bytes.fromhex("500e"), "reporting service port 443 to 80, low byte"),
    (0x5526, bytes.fromhex("010e"), bytes.fromhex("000e"), "reporting service port 443 to 80, high byte"),
    (0x54EA, bytes.fromhex("17c453f318c454f3"),
     bytes.fromhex("500e536f000e546f"), "cached service port 443 to 80"),
)


def loaded_bytes(memory):
    if len(memory) != 0x1FFFE or min(memory) != 0 or max(memory) != 0x1FFFD:
        raise ValueError("OEM HEX layout differs from the inspected 131070-byte image")
    return bytes(memory[i] for i in range(0x1FFFE))


def apply_patch(memory):
    before = loaded_bytes(memory)
    if hashlib.sha256(before).hexdigest() != OEM_LOADED_SHA256:
        raise ValueError("OEM loaded bytes do not match the canonical image")
    original = dict(memory)
    changes = []
    for address, expected, replacement, reason in PATCHES:
        actual = bytes(memory[address + i] for i in range(len(expected)))
        if actual != expected or len(expected) != len(replacement):
            raise ValueError(f"patch preimage mismatch at {address:#x}")
        for i, value in enumerate(replacement):
            memory[address + i] = value
        changes.append({"address": f"0x{address:06X}", "before": expected.hex(),
                        "after": replacement.hex(), "reason": reason})
    after = loaded_bytes(memory)
    changed = [f"0x{i:06X}" for i, (a, b) in enumerate(zip(before, after)) if a != b]
    if len(changed) != 12:
        raise ValueError(f"expected exactly 12 changed bytes, got {len(changed)}")

    # These identifiers and the regional NTP hostname must remain stock.
    if before[0x11CAC:0x11CBC] != b"uk.pool.ntp.org\0":
        raise ValueError("canonical OEM NTP hostname is missing")
    if after[0x11CAC:0x11CBC] != b"uk.pool.ntp.org\0":
        raise ValueError("patch changed the OEM NTP hostname")
    for address in (0x12147, 0x12179, 0x121A9):
        if b"sensornet.info\0" not in after[address:address + 48]:
            raise ValueError(f"OEM service hostname changed near {address:#x}")
    outside_patch = set(original) - {int(a, 16) + i for item in changes
                                     for i in range(len(bytes.fromhex(item["before"])))
                                     for a in [item["address"]]}
    if any(original[a] != memory[a] for a in outside_patch):
        raise ValueError("unexpected bytes changed outside the SSL/port patches")
    return changes, changed, hashlib.sha256(after).hexdigest()


def build(source, outdir):
    source = Path(source)
    raw_source = source.read_bytes()
    source_sha = hashlib.sha256(raw_source).hexdigest()
    if source_sha != OEM_FILE_SHA256:
        raise ValueError("input must be the unmodified OEM 3.7.1 full PIC HEX")
    memory = read_hex(source)
    if len(memory) != 0x1FFFE:
        raise ValueError("unexpected OEM HEX memory map")
    changes, changed, loaded_sha = apply_patch(memory)
    outdir = Path(outdir)
    outdir.mkdir(parents=True, exist_ok=True)
    full_hex = outdir / "oem-3.7.1-http-pickit.hex"
    update_hex = outdir / "oem-3.7.1-http-update.hex"
    write_hex(full_hex, memory)
    # Use the same stock-loader application layout as the OEM restore packager:
    # omit the protected loader, put the original app entry at address zero, and
    # retain OEM code/configuration regions that the OEM updater expects.
    _, app = stock_images(full_hex)
    write_hex(update_hex, app)
    manifest = {
        "format": "EFERGY-OEM-HTTP-PATCH-1",
        "model": "EH433M3v1",
        "oem_version": "3.7.1",
        "description": "OEM application with SSL client use bypassed; former SSL service requests use HTTP port 80",
        "source_file_sha256": source_sha,
        "source_loaded_sha256": OEM_LOADED_SHA256,
        "patched_loaded_sha256": loaded_sha,
        "full_pickit_hex_sha256": hashlib.sha256(full_hex.read_bytes()).hexdigest(),
        "oem_update_hex_sha256": hashlib.sha256(update_hex.read_bytes()).hexdigest(),
        "changed_byte_count": len(changed),
        "changed_byte_addresses": changed,
        "patches": changes,
        "preserved": {
            "ntp_hostname": "uk.pool.ntp.org",
            "service_hostnames": ["h3.sensornet.info", "update.sensornet.info", "keys.sensornet.info"],
            "all other loaded bytes": True,
        },
        "transport": "The OEM Docker server must offer the stock first-stage SSLv3 exchange on TCP 443; the patched application's service connections use HTTP on TCP 80.",
        "hardware_validation": "not performed by this packaging step",
    }
    (outdir / "oem-3.7.1-http-patch.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("oem_hex", type=Path, help="canonical OEM 3.7.1 PICkit HEX")
    parser.add_argument("--out", type=Path, default=Path("build/oem-http"))
    args = parser.parse_args()
    print(json.dumps(build(args.oem_hex, args.out), indent=2))


if __name__ == "__main__":
    main()
