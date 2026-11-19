#!/bin/sh
# Board-only single-thread UVC/YUYV -> RKNN test. No GUI unless requested.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
device=${1:-${CAR_CAMERA_DEVICE:-auto}}
frames=${2:-100}
display=${3:-0}
case "$frames" in ''|*[!0-9]*) printf 'Frame count must be a positive integer.\n' >&2; exit 2;; esac
case "$display" in 0|1) ;; *) printf 'Display must be 0 or 1.\n' >&2; exit 2;; esac
sh "$repo/scripts/build-npu-vision.sh"
(cd "$repo/vision/models" && sha256sum -c SHA256SUMS)
exec python3 "$repo/scripts/npu/run_camera.py" --binary "$repo/build/vision/detect_image" \
    --model "$repo/vision/models/yolov5n-rk3576-int8.rknn" --device "$device" \
    --output "$repo/build/vision/result-camera.jpg" --frames "$frames" --display "$display"
