#!/usr/bin/env python3
"""Install a prevalidated overlay, preserving the uEnv symlink and other settings."""
import fcntl
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tempfile
from datetime import datetime, timezone

OVERLAY_NAME = "car-lubancat3-gpio4-a6-key.dtbo"
OVERLAY_LINE = f"dtoverlay=/dtb/overlay/{OVERLAY_NAME}"


def updated_config(text, kernel_release):
    """Accept the known vendor layout and insert exactly one overlay entry."""
    lines = text.splitlines(keepends=True)
    stripped = [line.strip() for line in lines]
    for key, expected in (("uname_r", kernel_release), ("enable_uboot_overlays", "1")):
        values = [line.split("=", 1)[1].strip() for line in stripped
                  if "=" in line and line.split("=", 1)[0].strip() == key]
        if values != [expected]:
            raise ValueError(f"Expected exactly one {key}={expected}; got {values!r}")
    if stripped.count("#overlay_start") != 1 or stripped.count("#overlay_end") != 1:
        raise ValueError("Cannot identify one vendor #overlay_start/#overlay_end section")
    start, end = stripped.index("#overlay_start"), stripped.index("#overlay_end")
    if start >= end:
        raise ValueError("Overlay section markers are reversed")
    matches = []
    for index, line in enumerate(stripped):
        if not line.startswith("#") and OVERLAY_NAME in line:
            if line != OVERLAY_LINE:
                raise ValueError("Existing project overlay entry has an unexpected format")
            matches.append(index)
    if matches:
        if len(matches) != 1 or not start < matches[0] < end:
            raise ValueError("Existing project overlay entry is duplicated or outside its section")
        return text
    newline = "\r\n" if "\r\n" in text else "\n"
    lines.insert(end, OVERLAY_LINE + newline)
    return "".join(lines)


def atomic_write(path, contents, file_stat=None):
    """Write and fsync a sibling, replace, and fsync its parent directory."""
    fd, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as output:
            output.write(contents)
            os.fchmod(output.fileno(), stat.S_IMODE(file_stat.st_mode) if file_stat else 0o644)
            if file_stat and os.geteuid() == 0:
                os.fchown(output.fileno(), file_stat.st_uid, file_stat.st_gid)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, path)
        parent_fd = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY)
        try:
            os.fsync(parent_fd)
        finally:
            os.close(parent_fd)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def deploy(config, overlay_dir, source, kernel_release):
    original = config.read_bytes()
    updated = updated_config(original.decode("utf-8"), kernel_release).encode("utf-8")
    if len(updated) > 0x8000:
        raise ValueError("Configuration exceeds the vendor boot script's 0x8000-byte import limit")
    data = source.read_bytes()
    if len(data) < 40 or data[:4] != b"\xd0\x0d\xfe\xed":
        raise ValueError("Overlay is not a flattened device tree")
    if not overlay_dir.is_dir():
        raise ValueError("Vendor /boot/dtb/overlay directory does not exist")
    destination = overlay_dir / OVERLAY_NAME
    if destination.is_symlink() or (destination.exists() and not destination.is_file()):
        raise ValueError("Overlay destination is not a regular file")
    previous = destination.read_bytes() if destination.exists() else None
    if previous == data and original == updated:
        print("ALREADY INSTALLED: configuration and overlay are unchanged; reboot is still required.")
        return
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    backup = config.with_name(f"{config.name}.car-key-backup-{stamp}")
    shutil.copy2(config, backup)
    if previous is not None:
        shutil.copy2(destination, destination.with_name(f"{destination.name}.backup-{stamp}"))
    # Make backups durable before installing either file.
    os.sync()
    print(f"Configuration backup: {backup}", flush=True)
    config_stat = config.stat()
    overlay_stat = destination.stat() if destination.exists() else None
    # Install the blob before activating its reference in uEnv.
    try:
        atomic_write(destination, data, overlay_stat)
        if updated != original:
            atomic_write(config, updated, config_stat)
        if config.read_bytes() != updated or destination.read_bytes() != data:
            raise RuntimeError("Read-back verification failed")
    except BaseException:
        # Restore both files even if replacement succeeded before an fsync error.
        atomic_write(config, original, config_stat)
        if previous is None:
            destination.unlink(missing_ok=True)
        else:
            atomic_write(destination, previous, overlay_stat)
        os.sync()
        raise
    os.sync()
    print(f"Overlay installed: {destination}")
    print(f"Active configuration: {config}")
    print(OVERLAY_LINE)
    print("DEPLOYED: boot files verified. Power off, wire the key, then power on.")


def main():
    if len(sys.argv) != 2 or os.geteuid() != 0:
        raise ValueError("Run this helper as root with the prevalidated overlay file")
    source = Path(sys.argv[1]).resolve(strict=True)
    selector = Path("/boot/uEnv/uEnv.txt")
    expected = Path("/boot/uEnv/uEnvLubanCat3.txt")
    if selector.resolve(strict=True) != expected or expected.is_symlink():
        raise ValueError("Boot selection differs from the confirmed LubanCat-3 layout")
    if not expected.is_file():
        raise ValueError("Active configuration is not a regular file")
    kernel = subprocess.check_output(["uname", "-r"], text=True).strip()
    # Serialize this project's installers without replacing the selection symlink.
    with (expected.parent / ".car-gpio-key.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        deploy(expected, Path("/boot/dtb/overlay"), source, kernel)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"STOP: {error}", file=sys.stderr)
        sys.exit(1)
