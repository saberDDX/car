#!/usr/bin/env python3
"""Read-only BSP investigation; no full cmdline, environment or serial dump."""
from pathlib import Path
import datetime
import gzip
import os
import platform
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
LINES = []


def emit(text=""):
    LINES.append(text)
    print(text, flush=True)


def read(path):
    try:
        return Path(path).read_text(errors="replace").replace("\0", "\n")
    except OSError as error:
        return f"[unavailable: {error.strerror}]"


def command(args):
    if not shutil.which(args[0]):
        emit(f"{args[0]}: unavailable")
        return
    try:
        result = subprocess.run(args, capture_output=True, text=True, timeout=15)
        emit(result.stdout.strip())
        if result.returncode:
            emit(f"{args[0]} exit={result.returncode}: {result.stderr.strip()}")
    except (OSError, subprocess.TimeoutExpired) as error:
        emit(f"{args[0]}: {error}")


def main():
    kernel = platform.release()
    emit("BSP INVESTIGATION — reports configuration; does not establish recovery readiness")
    emit("UTC: " + datetime.datetime.now(datetime.timezone.utc).isoformat())
    emit(f"Architecture: {platform.machine()}  Kernel: {kernel}")
    emit("Model: " + read("/sys/firmware/devicetree/base/model").strip())
    emit("Compatible: " + read("/sys/firmware/devicetree/base/compatible").strip())
    emit("Kernel compiler: " + read("/proc/version").strip())
    for line in read("/etc/os-release").splitlines():
        if line.startswith(("PRETTY_NAME=", "VERSION_ID=")):
            emit(line)
    emit("\n[1] Storage and resources (no serial numbers)")
    command(["findmnt", "-n", "-o", "SOURCE,FSTYPE,TARGET", "/"])
    command(["findmnt", "-n", "-o", "SOURCE,FSTYPE,TARGET", "-T", "/boot"])
    command(["lsblk", "-o", "NAME,TYPE,SIZE,FSTYPE,MOUNTPOINT"])
    command(["df", "-h", str(ROOT), "/boot"])
    command(["free", "-h"])
    emit("\n[2] Selected kernel command-line fields")
    for token in read("/proc/cmdline").split():
        if token.split("=", 1)[0] in {"console", "root", "rootfstype", "rootwait", "ro", "rw", "init", "rdinit"}:
            # Root UUID/PARTUUID is a filesystem identifier, not a credential.
            emit(token)
    emit("\n[3] Boot candidates (presence does not prove bootloader selected them)")
    for name in ("/boot/uEnv/uEnv.txt", "/boot/uEnv/uEnvLubanCat3.txt", "/boot/extlinux/extlinux.conf"):
        path = Path(name)
        emit(f"{name} -> {path.resolve()}")
        content = read(path)
        if content.startswith("[unavailable:"):
            emit(content)
            continue
        for line in content.splitlines():
            if re.match(r"^\s*(uname_r|enable_uboot_overlays|dtoverlay|fdtfile|kernel_image|initrd_image)\s*=", line):
                emit(line.strip())
            elif re.match(r"^\s*(DEFAULT|LABEL|LINUX|KERNEL|INITRD|FDT|FDTDIR|FDTOVERLAYS|DEVICETREE)\s+", line, re.I):
                emit(line.strip())
    for directory in ("/boot", "/boot/dtb", "/boot/dtb/rockchip"):
        emit(f"Files in {directory}:")
        try:
            entries = sorted(Path(directory).iterdir())
            for path in entries[:100]:
                detail = " -> " + os.readlink(path) if path.is_symlink() else ""
                emit("  " + path.name + detail)
            if len(entries) > 100:
                emit("  [first 100 entries only]")
        except OSError as error:
            emit(f"  unavailable: {error.strerror}")
    emit("\n[4] Configuration sources and selected options")
    options = re.compile(r"^(?:# )?CONFIG_(?:LOCALVERSION(?:_AUTO)?|CC_VERSION_TEXT|IKCONFIG(?:_PROC)?|MODULES|MODVERSIONS|MODULE_SIG_FORCE|BLK_DEV_INITRD|I2C|I2C_RK3X|I2C_CHARDEV|OF_OVERLAY|EXT4_FS|MMC|MMC_DW_ROCKCHIP|MMC_SDHCI_OF_DWCMSHC)(?:=| is not set)")
    for name in (f"/boot/config-{kernel}", "/proc/config.gz", f"/lib/modules/{kernel}/build/.config", f"/lib/modules/{kernel}/build/include/config/auto.conf"):
        emit(name + ":")
        try:
            content = gzip.open(name, "rt").read() if name.endswith(".gz") else Path(name).read_text()
            emit("\n".join(line for line in content.splitlines() if options.match(line)))
        except OSError as error:
            emit("  unavailable: " + str(error))
    emit("\n[5] Installed BSP packages")
    command(["dpkg-query", "-W", "-f=${Package} ${Version}\n", "linux-image*", "linux-headers*", "u-boot*", "*lubancat*"])
    emit("\n[6] Runtime I2C and project state")
    for suffix in ("i2c@2aca0000/status", "i2c@2aca0000/mpu6050@68/compatible"):
        emit(suffix + ": " + read("/sys/firmware/devicetree/base/" + suffix).strip())
    for node in sorted(Path("/sys/bus/i2c/devices").glob("*-0068")):
        emit(f"{node.name}: of_node={node.joinpath('of_node').resolve()} driver={node.joinpath('driver').resolve() if node.joinpath('driver').exists() else 'unbound'}")
    command(["git", "-C", str(ROOT), "log", "-1", "--format=%h %s"])
    emit("\nRecovery hardware: must be confirmed by user; cannot infer from software.")
    output = ROOT / "build/bsp"
    output.mkdir(parents=True, exist_ok=True)
    report = output / "board-env.txt"
    report.write_text("\n".join(LINES) + "\n")
    print(f"\nReport saved: {report}")


if __name__ == "__main__":
    main()
