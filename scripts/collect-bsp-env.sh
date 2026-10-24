#!/bin/sh
# Read board configuration only; write a local report, never boot files.
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
exec python3 "$repo_dir/scripts/collect-bsp-env.py"
