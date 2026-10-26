#!/usr/bin/env python3
"""Inspect deployment inputs on the original board; write only a project report."""
from pathlib import Path
import gzip
import os
import platform
import re
import shutil
import subprocess


ROOT = Path(__file__).resolve().parents[1]
NEW_IMAGE_BYTES = 43057664
NEW_DTB_BYTES = 306930
RESERVE_BYTES = 16 * 1024 * 1024


def describe(path):
    try:
        link = f" -> {os.readlink(path)}" if path.is_symlink() else ""
        return f"{path}: {path.stat().st_size} bytes{link}"
    except OSError as error:
        return f"{path}: unavailable ({error.strerror})"


def inspect_initrd(path):
    """Summarize modules without dumping the initramfs or its configuration."""
    try:
        with path.open("rb") as stream:
            magic = stream.read(8).hex()
    except OSError as error:
        return f"Cannot read initrd: {error.strerror}; no sudo was attempted."
    if not shutil.which("lsinitramfs"):
        return f"Header={magic}; lsinitramfs unavailable."
    try:
        result = subprocess.run(["lsinitramfs", str(path)], capture_output=True,
                                text=True, timeout=15)
    except (OSError, subprocess.TimeoutExpired) as error:
        return f"Header={magic}; inspection unavailable: {error}"
    if result.returncode:
        return f"Header={magic}; lsinitramfs exit={result.returncode}: {result.stderr[-500:].strip()}"
    releases = sorted(set(re.findall(r"(?:^|/)lib/modules/([^/\s]+)", result.stdout, re.M)))
    return f"Header={magic}; embedded module releases={releases or 'none found'}"


def loader_candidates():
    """Bounded filename search in firmware locations; do not read binaries."""
    found = []
    visited = 0
    for base in (Path("/boot"), Path("/usr/lib/u-boot"), Path("/usr/share/u-boot"), Path("/opt")):
        if not base.is_dir():
            continue
        for directory, subdirs, files in os.walk(base, followlinks=False):
            relative = Path(directory).relative_to(base)
            subdirs[:] = sorted(name for name in subdirs if not name.startswith("."))
            if len(relative.parts) >= 3:
                subdirs[:] = []
            for name in sorted(files):
                visited += 1
                if visited > 3000:
                    return found, True
                lower = name.lower()
                if lower.endswith((".bin", ".img")) and ("loader" in lower or "usbplug" in lower):
                    found.append(describe(Path(directory) / name))
    return found, False


def main():
    try:
        model = Path("/sys/firmware/devicetree/base/model").read_text().rstrip("\0\n")
    except OSError:
        model = "unavailable"
    if platform.machine() != "aarch64" or model != "EmbedFire LubanCat-3" or platform.release() != "6.1.99-rk3576":
        raise SystemExit("STOP: run on the original LubanCat-3 / 6.1.99-rk3576. No files changed.")
    lines = []

    def emit(message=""):
        print(message, flush=True)
        lines.append(message)

    emit("BSP deployment input inspection — NOT an install or readiness approval")
    emit(f"Model: {model}; running kernel: {platform.release()}")
    emit("\n[1] Mounts and available space")
    for target in ("/boot", str(ROOT)):
        result = subprocess.run(["findmnt", "-n", "-o", "SOURCE,FSTYPE,TARGET", "-T", target],
                                capture_output=True, text=True, timeout=10)
        emit(result.stdout.strip() or f"findmnt unavailable for {target}: {result.stderr.strip()}")
        space = os.statvfs(target)
        emit(f"{target}: available={space.f_bavail * space.f_frsize} bytes")
    space = os.statvfs("/boot")
    free = space.f_bavail * space.f_frsize
    emit(f"New Image={NEW_IMAGE_BYTES}, DTB={NEW_DTB_BYTES}, provisional reserve={RESERVE_BYTES} bytes")
    emit(f"Remaining initrd budget with that reserve={free - NEW_IMAGE_BYTES - NEW_DTB_BYTES - RESERVE_BYTES} bytes")
    emit("This is a budget only; actual new initrd size must be measured before installation.")

    emit("\n[2] Boot files and selected uEnv fields")
    names = sorted(set(Path("/boot").glob("Image*")) | set(Path("/boot").glob("initrd*")))
    for path in names + [Path("/boot/rk-kernel.dtb"), Path("/boot/boot.scr"), Path("/boot/uEnv/uEnv.txt")]:
        emit(describe(path))
    uenv = Path("/boot/uEnv/uEnv.txt")
    initrd_name = None
    try:
        for line in uenv.read_text().splitlines():
            if re.match(r"^\s*(uname_r|initrd|size|enable_uboot_overlays|dtoverlay)\s*=", line):
                emit(line.strip())
            match = re.match(r"^\s*initrd\s*=\s*([^\s]+)\s*$", line)
            if match:
                initrd_name = match[1].strip("\"'")
    except OSError as error:
        emit(f"uEnv unreadable: {error}")
    emit("\n[3] Currently selected initrd")
    if initrd_name and not Path(initrd_name).is_absolute() and ".." not in Path(initrd_name).parts:
        initrd = Path("/boot") / initrd_name
        emit(describe(initrd))
        emit(inspect_initrd(initrd))
    else:
        emit("No supported initrd field found; manual inspection required.")

    emit("\n[4] Initramfs and deployment tools")
    for name in ("mkinitramfs", "lsinitramfs", "depmod", "modinfo", "gzip", "sha256sum", "mkimage", "fw_printenv"):
        emit(f"{name}: {shutil.which(name) or 'unavailable'}")
    for config in (Path("/etc/initramfs-tools/initramfs.conf"), Path("/etc/initramfs-tools/modules")):
        emit(str(config))
        try:
            for line in config.read_text().splitlines():
                clean = line.strip()
                if clean and not clean.startswith("#") and (config.name == "modules" or clean.startswith(("MODULES=", "COMPRESS=", "BUSYBOX="))):
                    emit("  " + clean)
        except OSError as error:
            emit(f"  unavailable: {error.strerror}")
    try:
        with gzip.open("/proc/config.gz", "rt") as stream:
            for line in stream:
                if re.match(r"^(?:# )?CONFIG_(?:KEXEC|KEXEC_FILE|BLK_DEV_INITRD|EXT4_FS|MMC|MMC_SDHCI|MMC_SDHCI_PLTFM|MMC_SDHCI_OF_DWCMSHC)(?:=| is not set)", line):
                    emit(line.strip())
    except OSError as error:
        emit(f"Kernel configuration unavailable: {error.strerror}")

    emit("\n[5] Existing loader filename candidates (unverified)")
    candidates, truncated = loader_candidates()
    for entry in candidates:
        emit(entry)
    if not candidates:
        emit("None found in bounded firmware locations; this does not prove none exist elsewhere.")
    if truncated:
        emit("Search stopped at 3000 filenames.")
    emit("\nOnly inspected system inputs; no boot files, modules, packages or block devices changed.")
    output = ROOT / "build/bsp/deployment-inputs.txt"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines) + "\n")
    print(f"Report: {output}")


if __name__ == "__main__":
    main()
