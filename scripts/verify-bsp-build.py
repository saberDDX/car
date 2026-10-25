#!/usr/bin/env python3
"""Verify offline ARM64 build artifacts. This does not establish boot success."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess


def digest(path):
    checksum = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            checksum.update(block)
    return checksum.hexdigest()


def verify(root, readelf, fdtget):
    release = (root / "include/config/kernel.release").read_text().strip()
    if release != "6.1.99-rk3576-car-bsp":
        raise ValueError("unexpected experimental kernel release")
    image = root / "arch/arm64/boot/Image"
    with image.open("rb") as handle:
        header = handle.read(64)
    if len(header) != 64 or header[56:60] != b"ARM\x64":
        raise ValueError("not an ARM64 Linux Image")
    module = root / "drivers/misc/car_mpu6050/car_mpu6050.ko"
    with module.open("rb") as handle:
        elf = handle.read(20)
    if len(elf) != 20 or elf[:6] != b"\x7fELF\x02\x01" or struct.unpack("<H", elf[18:20])[0] != 183:
        raise ValueError("MPU6050 module is not little-endian ELF64 AArch64")
    info = subprocess.check_output([readelf, "-p", ".modinfo", str(module)], text=True)
    if f"vermagic={release} " not in info or "saberddx,car-mpu6050" not in info:
        raise ValueError("module release or OF alias mismatch")
    dtb = root / "arch/arm64/boot/dts/rockchip/rk3576-lubancat-3.dtb"
    model = subprocess.check_output([fdtget, "-t", "s", str(dtb), "/", "model"], text=True).strip()
    if model != "EmbedFire LubanCat-3":
        raise ValueError("wrong board DTB")
    modules = (root / "modules.order").read_text().splitlines()
    missing = [name for name in modules if not root.joinpath(name).is_file()]
    if not modules or missing:
        raise ValueError(f"missing module artifacts: {missing[:5]}")
    return {
        "release": release, "architecture": "AArch64", "module_count": len(modules),
        "board_model": model,
        "files": {str(p.relative_to(root)): {"bytes": p.stat().st_size, "sha256": digest(p)}
                  for p in (image, module, dtb, root / ".config")},
        "scope": "Offline artifact validation only; not installed or boot-tested.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--readelf", default="aarch64-linux-gnu-readelf")
    parser.add_argument("--fdtget", default="fdtget")
    args = parser.parse_args()
    print(json.dumps(verify(args.output, args.readelf, args.fdtget), indent=2))
