#!/usr/bin/env python3
"""Run detection unprivileged; temporarily grant only the selected camera ACL."""
import argparse
from contextlib import contextmanager
import fcntl
import os
from pathlib import Path
import re
import shutil
import signal
import stat
import subprocess
import sys
import tempfile

def ensure_sudo():
    if shutil.which("sudo") is None:
        raise RuntimeError("camera access requires sudo for scoped device setup; sudo is unavailable")
    # Keep the authentication prompt visible, outside captured probe diagnostics.
    subprocess.run(["sudo", "-v"], check=True)

def root_command(arguments, **kwargs):
    return subprocess.run(["sudo", "-n", *map(str, arguments)], check=True, **kwargs)

def device_identity(path):
    info = os.stat(path)
    return info.st_dev, info.st_ino, info.st_rdev

def resolve_device(text):
    lines = [line.strip() for line in text.splitlines() if line.strip()]
    if len(lines) != 1:
        raise RuntimeError("camera probe must return exactly one device path")
    path = Path(lines[0]).resolve(strict=True)
    if not path.is_relative_to(Path("/dev")) or not stat.S_ISCHR(path.stat().st_mode):
        raise RuntimeError("probe returned a non-device path")
    return str(path)

def select_camera(binary, requested):
    arguments = [str(binary), "--find-camera"] if requested == "auto" else [str(binary), "--probe-camera", requested]
    result = subprocess.run(arguments, capture_output=True, text=True)
    if result.stderr:
        print(result.stderr, file=sys.stderr, end="")
    if result.returncode:
        if not re.search(r"open_errno=(?:1|13)(?!\d)", result.stderr):
            raise RuntimeError("camera discovery failed; diagnostics above identify the device/format error")
        print("Camera open was denied. Requesting sudo for metadata-only discovery, not NPU inference.", file=sys.stderr)
        ensure_sudo()
        try:
            result = root_command(arguments, capture_output=True, text=True)
        except subprocess.CalledProcessError as error:
            if error.stderr:
                print(error.stderr, file=sys.stderr, end="")
            raise RuntimeError("privileged metadata probe also failed; no camera permissions were changed") from error
        if result.stderr:
            print(result.stderr, file=sys.stderr, end="")
    return resolve_device(result.stdout)

@contextmanager
def camera_access(device):
    if os.access(device, os.R_OK | os.W_OK):
        print(f"camera_access=existing device={device}", flush=True)
        yield
        return
    for tool in ("getfacl", "setfacl"):
        if shutil.which(tool) is None:
            raise RuntimeError(f"{tool} is missing (Ubuntu package: acl)")
    ensure_sudo()
    identity = device_identity(device)
    directory = Path(tempfile.mkdtemp(prefix="car-npu-acl-"))
    backup = directory / "camera.acl"
    captured = False
    try:
        with backup.open("w") as output:
            root_command(["getfacl", "-p", "-n", "--", device], stdout=output)
        captured = True
        if device_identity(device) != identity:
            raise RuntimeError("camera device changed before permission setup")
        root_command(["setfacl", "-m", f"u:{os.getuid()}:rw", "--", device])
        if device_identity(device) != identity or not os.access(device, os.R_OK | os.W_OK):
            raise RuntimeError("camera changed or remains inaccessible after scoped ACL setup")
        print(f"camera_access=temporary_acl device={device} uid={os.getuid()}", flush=True)
        yield
    finally:
        previous = {number: signal.signal(number, signal.SIG_IGN) for number in (signal.SIGINT, signal.SIGTERM)}
        try:
            if captured:
                try:
                    if device_identity(device) != identity:
                        raise RuntimeError("device identity changed; refusing to restore an old ACL onto a new device")
                    root_command(["setfacl", f"--restore={backup}"])
                    print(f"camera_acl=restored device={device}", flush=True)
                except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
                    raise RuntimeError(f"ACL restore incomplete: {error}. Original ACL retained at {backup}") from error
            shutil.rmtree(directory)
        finally:
            for number, handler in previous.items():
                signal.signal(number, handler)

def run_detector(command):
    child = None
    received = [0]
    def forward(number, _frame):
        received[0] = number
        if child is None:
            raise KeyboardInterrupt
        if child.poll() is None:
            try:
                child.send_signal(number)
            except ProcessLookupError:
                pass
    previous = {number: signal.signal(number, forward) for number in (signal.SIGINT, signal.SIGTERM)}
    try:
        child = subprocess.Popen(command)  # No sudo: capture and inference remain the current user.
        status = child.wait()
        return 128 + received[0] if received[0] else status
    finally:
        for number in previous:
            signal.signal(number, signal.SIG_IGN)
        if child is not None and child.poll() is None:
            child.terminate()
            try:
                child.wait(timeout=5)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait()
        for number, handler in previous.items():
            signal.signal(number, handler)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--device", default="auto")
    parser.add_argument("--model", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--frames", type=int, default=100)
    parser.add_argument("--display", choices=["0", "1"], default="0")
    args = parser.parse_args()
    if os.geteuid() == 0:
        raise RuntimeError("run this launcher as your normal user, without sudo; it requests sudo only for camera setup")
    if not 1 <= args.frames <= 10000:
        raise ValueError("frame count must be 1..10000")
    binary = args.binary.resolve(strict=True)
    with (binary.parent / ".camera-run.lock").open("a") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise RuntimeError("another NPU camera launcher is already running") from error
        device = select_camera(binary, args.device)
        with camera_access(device):
            return run_detector([str(binary), "--model", args.model, "--camera", device,
                                 "--output", args.output, "--warmup", "5", "--repeat", str(args.frames),
                                 "--display", args.display])

if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)
