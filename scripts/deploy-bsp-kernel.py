#!/usr/bin/env python3
"""Stage the pinned BSP package, then explicitly activate or restore uEnv."""
import argparse
import base64
import fcntl
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path, PurePosixPath
import platform
import shutil
import struct
import subprocess
import sys
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]
OLD = "6.1.99-rk3576"
NEW = "6.1.99-rk3576-car-bsp"
PACKAGE_SHA256 = "a7e612fe27e8151abd5d85c37accc67f24045d02891b9ab09658a503ddeee6e5"
UENV = "boot/uEnv/uEnvLubanCat3.txt"
STATE = "var/lib/car-bsp/" + NEW + "/state.json"
PINNED = {
    "boot/boot.cmd": "0cd147b562ec84364c00e300d9b2b18d2383eaefb63b958676d17a3102bcb57d",
    "boot/boot.scr": "0abc9e2b5a49e42ce469b0799fd911cbb68862e9caa5b6f397afc5fd302a63f8",
    "boot/initrd-6.1": "a866ace9754b86b9fd552ecfe16f14de5386246f8bf2e537b28634916d55768a",
    "boot/dtb/rk3576-lubancat-3.dtb": "ad6dffba0a66463f6b9bb6fdc9a857c51d059904f636f45043e6e353f3729b96",
}
UENV_SHA256 = "86ee0ba24592390a2239a965c181e278b7eb114fc142958174605366bd1f0ccf"
NEW_BOOT = {
    f"boot/Image-{NEW}": "75387713f8cf3c282c76e17c356c6625a07b90af41013abe5148e8f13f216d5d",
    f"boot/initrd-{NEW}": "b3d8ef3de3ef0fbf32630f3165c426f5a8fe36dca6d1912be77d5511dbaa6c4b",
}
spec = importlib.util.spec_from_file_location("bsp_atomic", ROOT / "scripts/install-key-overlay.py")
atomic_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(atomic_module)
atomic_write = atomic_module.atomic_write


def digest(data):
    return hashlib.sha256(data).hexdigest()


def safe_name(name):
    path = PurePosixPath(name)
    if path.is_absolute() or ".." in path.parts or str(path) != name:
        raise ValueError("Noncanonical or unsafe archive/state path")
    return name


def load_package(path):
    with path.open("rb") as source:
        blob = source.read(64 * 1024 * 1024 + 1)
    if len(blob) > 64 * 1024 * 1024 or digest(blob) != PACKAGE_SHA256:
        raise ValueError("Package SHA256 mismatch; use car-bsp-artifacts-with-initrd.tar.gz")
    files = {}
    total = 0
    with tarfile.open(fileobj=io.BytesIO(blob), mode="r:gz") as archive:
        seen = set()
        for entry in archive:
            name = safe_name(entry.name)
            if name in seen or not (entry.isdir() or entry.isfile()):
                raise ValueError("Duplicate or nonregular archive member")
            seen.add(name)
            if entry.isdir():
                continue
            total += entry.size
            if total > 256 * 1024 * 1024 or entry.size > 128 * 1024 * 1024:
                raise ValueError("Archive exceeds staging limits")
            with archive.extractfile(entry) as stream:
                files[name] = stream.read()
    checksum_name = "provenance/SHA256SUMS"
    listed = {}
    for line in files[checksum_name].decode().splitlines():
        checksum, name = line.split("  ", 1)
        safe_name(name)
        if name in listed or name not in files or digest(files[name]) != checksum:
            raise ValueError("Package file manifest mismatch")
        listed[name] = checksum
    if set(listed) != set(files) - {checksum_name}:
        raise ValueError("Incomplete package manifest")
    for name, expected in NEW_BOOT.items():
        if digest(files[name]) != expected:
            raise ValueError("Pinned Image/initrd mismatch")
    if files[f"boot/Image-{NEW}"][56:60] != b"ARM\x64":
        raise ValueError("Not an ARM64 Image")
    modules = {name: data for name, data in files.items() if name.startswith(f"lib/modules/{NEW}/")}
    ko = [data for name, data in modules.items() if name.endswith(".ko")]
    if len(ko) != 257:
        raise ValueError("Expected 257 matching kernel modules")
    for data in ko:
        if len(data) < 20 or data[:6] != b"\x7fELF\x02\x01" or struct.unpack_from("<H", data, 18)[0] != 183:
            raise ValueError("Packaged module is not AArch64")
        if b"vermagic=" + NEW.encode() + b" " not in data:
            raise ValueError("Packaged module has the wrong vermagic")
    if digest(files["boot/dtb/car-bsp/rk3576-lubancat-3.dtb"]) != PINNED["boot/dtb/rk3576-lubancat-3.dtb"]:
        raise ValueError("Packaged DTB differs from the original shared DTB")
    return {**modules, **{name: files[name] for name in NEW_BOOT}}


def assert_file(path, expected=None):
    if path.is_symlink() or not path.is_file():
        raise ValueError(f"Expected a regular file: {path}")
    if expected is not None and digest(path.read_bytes()) != expected:
        raise ValueError(f"File changed or is incomplete: {path}")


def check_baseline(root):
    if (root / "boot/uEnv/uEnv.txt").resolve(strict=True) != root / UENV:
        raise ValueError("Unexpected uEnv selection")
    if (root / "boot/rk-kernel.dtb").resolve(strict=True) != root / "boot/dtb/rk3576-lubancat-3.dtb":
        raise ValueError("Unexpected base DTB selection")
    for name, checksum in PINNED.items():
        assert_file(root / name, checksum)
    assert_file(root / f"boot/Image-{OLD}")
    assert_file(root / UENV)


def experiment_uenv(original):
    if digest(original) != UENV_SHA256:
        raise ValueError("uEnv differs from audited original configuration")
    updated = original
    for old, new in ((f"uname_r={OLD}\n", f"uname_r={NEW}\n"),
                     ("initrd=initrd-6.1\n", f"initrd=initrd-{NEW}\n")):
        if updated.count(old.encode()) != 1:
            raise ValueError("Cannot uniquely replace known uEnv field")
        updated = updated.replace(old.encode(), new.encode())
    if len(updated) > 0x8000:
        raise ValueError("uEnv exceeds vendor import limit")
    return updated


def load_state(root):
    assert_file(root / STATE)
    state = json.loads((root / STATE).read_text())
    if state["release"] != NEW or state["package_sha256"] != PACKAGE_SHA256:
        raise ValueError("Staging state belongs to another build")
    original = base64.b64decode(state["original_uenv"], validate=True)
    experiment_uenv(original)
    return state, original


def save_state(root, state):
    destination = root / STATE
    destination.parent.mkdir(parents=True, exist_ok=True)
    atomic_write(destination, (json.dumps(state, indent=2) + "\n").encode())


def verify_installed(root, state):
    for name, checksum in state["installed_files"].items():
        safe_name(name)
        if name not in NEW_BOOT and not name.startswith(f"lib/modules/{NEW}/"):
            raise ValueError("Unexpected installation target in state")
        assert_file(root / name, checksum)
    for name, checksum in state["old_boot_files"].items():
        if name not in PINNED and name != f"boot/Image-{OLD}":
            raise ValueError("Unexpected old boot target in state")
        assert_file(root / name, checksum)


def stage(root, files):
    """Install independently named artifacts. Never modify active boot selection."""
    check_baseline(root)
    original = (root / UENV).read_bytes()
    updated = experiment_uenv(original)
    wanted = {name: digest(data) for name, data in files.items()}
    state_path = root / STATE
    if state_path.exists():
        state, saved = load_state(root)
        if saved != original or state["installed_files"] != wanted:
            raise ValueError("Existing state/configuration conflicts with this staging request")
    else:
        old_files = {name: digest((root / name).read_bytes())
                     for name in [*PINNED, f"boot/Image-{OLD}"]}
        state = {"release": NEW, "package_sha256": PACKAGE_SHA256, "phase": "preparing",
                 "original_uenv": base64.b64encode(original).decode(),
                 "installed_files": wanted, "old_boot_files": old_files}
    needed = 0
    for name in NEW_BOOT:
        path = root / name
        if path.exists() or path.is_symlink():
            assert_file(path, wanted[name])
        else:
            needed += ((len(files[name]) + 4095) // 4096) * 4096
    space = os.statvfs(root / "boot")
    if space.f_bavail * space.f_frsize < needed + 8 * 1024 * 1024:
        raise ValueError("Insufficient /boot space with 8MiB remaining reserve; old files will not be deleted")
    if shutil.disk_usage(root / "lib/modules").free < sum(len(data) for data in files.values()) + 8 * 1024 * 1024:
        raise ValueError("Insufficient root filesystem staging space")
    save_state(root, state)
    module_prefix = f"lib/modules/{NEW}/"
    target = root / f"lib/modules/{NEW}"
    if target.exists() or target.is_symlink():
        if target.is_symlink() or not target.is_dir():
            raise ValueError("New module directory is not a regular directory")
        actual = {p.relative_to(root).as_posix() for p in target.rglob("*") if p.is_file() or p.is_symlink()}
        if actual != {name for name in files if name.startswith(module_prefix)}:
            raise ValueError("Existing new module tree has unexpected/missing files")
        for name, checksum in wanted.items():
            if name.startswith(module_prefix):
                assert_file(root / name, checksum)
    else:
        temporary = Path(tempfile.mkdtemp(prefix=".car-bsp-stage-", dir=target.parent))
        try:
            temporary.chmod(0o755)
            for name, data in files.items():
                if not name.startswith(module_prefix):
                    continue
                path = temporary / name.removeprefix(module_prefix)
                path.parent.mkdir(parents=True, exist_ok=True)
                atomic_write(path, data)
            for path in temporary.rglob("*"):
                if path.is_dir():
                    path.chmod(0o755)
            os.replace(temporary, target)
            parent_fd = os.open(target.parent, os.O_RDONLY | os.O_DIRECTORY)
            try:
                os.fsync(parent_fd)
            finally:
                os.close(parent_fd)
        finally:
            if temporary.exists():
                shutil.rmtree(temporary)
    for name in NEW_BOOT:
        if not (root / name).exists():
            atomic_write(root / name, files[name])
    verify_installed(root, state)
    if (root / UENV).read_bytes() != original:
        raise RuntimeError("Active configuration changed concurrently")
    state["phase"] = "staged"
    save_state(root, state)
    os.sync()
    print(f"STAGED: Image, initrd and 257 modules for {NEW}; active uEnv unchanged.")
    print(f"Prepared uEnv changes: uname_r={NEW}; initrd=initrd-{NEW}")
    print(f"State/rollback data: {root / STATE}")
    return state


def switch(root, rollback=False):
    check_baseline(root)
    state, original = load_state(root)
    updated = experiment_uenv(original)
    config = root / UENV
    current = config.read_bytes()
    if current not in (original, updated):
        raise ValueError("uEnv has unrelated changes; will not overwrite it")
    if rollback:
        for name, checksum in state["old_boot_files"].items():
            assert_file(root / safe_name(name), checksum)
        target = original
    else:
        if state["phase"] not in ("staged", "active", "rolled_back"):
            raise ValueError("Staging did not complete")
        verify_installed(root, state)
        target = updated
    previous_state = dict(state)
    try:
        if current != target:
            atomic_write(config, target, config.stat())
        if config.read_bytes() != target:
            raise RuntimeError("uEnv readback mismatch")
        state["phase"] = "rolled_back" if rollback else "active"
        save_state(root, state)
    except BaseException:
        if config.read_bytes() != current:
            atomic_write(config, current, config.stat())
        # State is diagnostic; restore it too if a rename happened before fsync failed.
        save_state(root, previous_state)
        raise
    os.sync()
    print(f"{'RESTORED original' if rollback else 'ACTIVATED experiment'} uEnv; not rebooted.")
    print("Original Image/initrd, DTB links, overlays, boot.cmd, boot.scr and extlinux unchanged.")


def ensure_board(allow_new=False):
    allowed = (OLD, NEW) if allow_new else (OLD,)
    if os.geteuid() != 0 or platform.machine() != "aarch64" or platform.release() not in allowed:
        raise ValueError("Use sudo on the verified LubanCat-3 running the original kernel (or experiment for rollback/status)")
    if Path("/sys/firmware/devicetree/base/model").read_bytes().rstrip(b"\0\n") != b"EmbedFire LubanCat-3":
        raise ValueError("Unexpected board model")
    for mount, device in (("/boot", "/dev/mmcblk0p2"), ("/", "/dev/mmcblk0p3")):
        actual = subprocess.check_output(["findmnt", "-n", "-o", "SOURCE", "-T", mount], text=True).strip()
        if actual != device:
            raise ValueError(f"Unexpected filesystem backing {mount}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("check-package", "stage", "activate", "rollback", "status"))
    parser.add_argument("package", nargs="?", type=Path,
                        default=ROOT / "build/bsp/car-bsp-artifacts-with-initrd.tar.gz")
    args = parser.parse_args()
    if args.action == "check-package":
        files = load_package(args.package)
        print(f"PASS: pinned package, manifest, ARM64 Image, initrd and 257 module checks; {len(files)} installation files.")
        return
    ensure_board(allow_new=args.action in ("rollback", "status"))
    with Path("/run/lock/car-bsp.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        if args.action == "stage":
            stage(Path("/"), load_package(args.package))
        elif args.action in ("activate", "rollback"):
            switch(Path("/"), rollback=args.action == "rollback")
        else:
            state, original = load_state(Path("/"))
            current = Path("/" + UENV).read_bytes()
            print(json.dumps({"running_kernel": platform.release(), "phase": state["phase"],
                              "uenv": "original" if current == original else
                              "experiment" if current == experiment_uenv(original) else "modified"}, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, RuntimeError, tarfile.TarError, subprocess.SubprocessError) as error:
        print(f"STOP: {error}", file=sys.stderr)
        sys.exit(1)
