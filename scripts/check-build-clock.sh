#!/bin/sh
# Read-only guard for known clock-skew failure; never alter source/header mtimes.
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
python3 - "$repo_dir" "/lib/modules/$(uname -r)/build" <<'PY'
from pathlib import Path
import datetime
import sys
import time

repo, headers = map(Path, sys.argv[1:])
paths = list((repo / "drivers/mpu6050").glob("*"))
paths += list((repo / "drivers/mpu6050").glob(".*.cmd"))
paths += list((repo / "include").glob("*mpu6050*"))
paths += list((repo / "tools/mpu6050-monitor").glob("*"))
paths += [headers / "Makefile", headers / "scripts/Makefile.gcc-plugins"]
def future_files():
    return [p for p in paths if p.is_file() and p.stat().st_mtime > time.time() + 300]

future = future_files()
if future:
    print("Waiting up to 30 seconds for the board clock to catch up via NTP...", flush=True)
    deadline = time.monotonic() + 30
    while future and time.monotonic() < deadline:
        time.sleep(1)
        future = future_files()
if future:
    print("STOP: board clock is behind build files; synchronize time before building.", file=sys.stderr)
    print("Board UTC:", datetime.datetime.now(datetime.timezone.utc).isoformat(), file=sys.stderr)
    for path in future[:5]:
        print("Future file:", path, file=sys.stderr)
    print("Run sudo timedatectl set-ntp true; inspect timedatectl status. "
          "If NTP is unavailable, set the real current time before retrying.", file=sys.stderr)
    sys.exit(1)
print("Build clock check passed (no >5-minute future timestamps in checked files).")
PY
