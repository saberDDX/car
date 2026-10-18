#!/usr/bin/env python3
"""Reuse the tested atomic installer, adding only the MPU6050 overlay entry."""
import fcntl
import importlib.util
import os
from pathlib import Path
import subprocess
import sys

OVERLAY_NAME = "car-lubancat3-mpu6050.dtbo"
SPEC = importlib.util.spec_from_file_location(
    "car_overlay_installer", Path(__file__).with_name("install-key-overlay.py"))
installer = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(installer)


def main():
    if len(sys.argv) != 2 or os.geteuid() != 0:
        raise ValueError("Run as root with the prevalidated MPU6050 overlay file")
    source = Path(sys.argv[1]).resolve(strict=True)
    selector = Path("/boot/uEnv/uEnv.txt")
    expected = Path("/boot/uEnv/uEnvLubanCat3.txt")
    if selector.resolve(strict=True) != expected or expected.is_symlink() or not expected.is_file():
        raise ValueError("Boot selection differs from the confirmed LubanCat-3 layout")
    kernel = subprocess.check_output(["uname", "-r"], text=True).strip()
    # Share the key installer's lock because both modify the same uEnv file.
    with (expected.parent / ".car-gpio-key.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        installer.deploy(expected, Path("/boot/dtb/overlay"), source, kernel,
                         overlay_name=OVERLAY_NAME, backup_tag="car-mpu6050",
                         instructions="Power off, wire the MPU6050, then power on.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"STOP: {error}", file=sys.stderr)
        sys.exit(1)
