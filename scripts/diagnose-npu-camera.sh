#!/bin/sh
# Inspect the saved result and capture fresh pixels. Does not load any RKNN model.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
sh "$repo/scripts/build-npu-vision.sh"
exec python3 "$repo/scripts/npu/diagnose_camera.py" --build-directory "$repo/build/vision" \
    --device "${1:-${CAR_CAMERA_DEVICE:-auto}}"
