#!/usr/bin/env python3
"""Capture-only diagnostics reuse the scoped camera permissions, without RKNN."""
import argparse
import fcntl
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from run_camera import camera_access, select_camera, run_detector

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-directory", type=Path, required=True)
    parser.add_argument("--device", default="auto")
    args = parser.parse_args()
    if os.geteuid() == 0:
        raise RuntimeError("run without sudo; scoped camera setup requests it only when needed")
    directory = args.build_directory.resolve(strict=True)
    with (directory / ".camera-run.lock").open("a") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise RuntimeError("close the other NPU camera launcher before diagnostics") from error
        root = directory / "camera-diagnostics"
        root.mkdir(exist_ok=True)
        output = Path(tempfile.mkdtemp(prefix="capture-", dir=root))
        latest = root / "latest"
        if latest.is_symlink():
            latest.unlink()
        elif latest.exists():
            raise RuntimeError(f"refusing to overwrite existing non-symlink {latest}")
        latest.symlink_to(output.name, target_is_directory=True)
        probe = directory / "camera_diagnostics"
        previous = directory / "result-camera.jpg"
        if previous.exists():
            result = subprocess.run([str(probe), "--inspect", str(previous)], capture_output=True, text=True)
            print(result.stdout, end="", flush=True)
            print(result.stderr, end="", file=sys.stderr)
            (output / "previous-result-inspection.txt").write_text(result.stdout + result.stderr)
        device = select_camera(directory / "detect_image", args.device)
        print(f"diagnostic_output={output}", flush=True)
        print(f"diagnostic_latest={latest}", flush=True)
        with camera_access(device):
            return run_detector([str(probe), device, str(output)])

if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)
