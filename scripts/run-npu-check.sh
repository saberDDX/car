#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cmake -S "$repo/tools/npu-check" -B "$repo/build/npu-check"
cmake --build "$repo/build/npu-check" --parallel 2
exec "$repo/build/npu-check/check_runtime" "${1:-/lib/librknnrt.so}"
