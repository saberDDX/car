#!/usr/bin/env python3
"""Analyze exported board files as data; do not execute boot scripts."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib


def analyze(root):
    config = (root / "running.config").read_bytes()
    installed = (root / "boot/config-6.1.99-rk3576").read_bytes()
    if config != installed:
        raise ValueError("running and installed configurations differ")
    command = (root / "boot/boot.cmd").read_bytes()
    script = (root / "boot/boot.scr").read_bytes()
    if len(script) < 72:
        raise ValueError("truncated legacy script image")
    header = bytearray(script[:64])
    magic, header_crc, timestamp, size, load, entry, data_crc = struct.unpack(">7I", header[:28])
    if magic != 0x27051956 or len(script) != size + 64:
        raise ValueError("invalid legacy script header or length")
    header[4:8] = b"\0" * 4
    payload = script[64:]
    if zlib.crc32(header) != header_crc or zlib.crc32(payload) != data_crc:
        raise ValueError("boot.scr CRC mismatch")
    component, terminator = struct.unpack(">II", payload[:8])
    if terminator or component != len(command) or payload[8:8 + component] != command:
        raise ValueError("boot.scr does not contain the supplied boot.cmd")
    fields, overlays = {}, []
    for line in (root / "boot/uEnv/uEnvLubanCat3.txt").read_text().splitlines():
        line = line.strip()
        if line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        if key == "dtoverlay":
            overlays.append(value)
        elif key in {"uname_r", "initrd", "enable_uboot_overlays"}:
            fields[key] = value
    return {
        "config_sha256": hashlib.sha256(config).hexdigest(),
        "running_equals_installed_config": True,
        "boot_scr_crc_valid": True,
        "boot_scr_matches_boot_cmd": True,
        "uenv": fields, "overlays": overlays,
        "note": "Static script analysis does not prove which U-Boot boot target was selected.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    print(json.dumps(analyze(args.directory), indent=2, ensure_ascii=False))
