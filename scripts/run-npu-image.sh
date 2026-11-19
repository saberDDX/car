#!/bin/sh
# Board smoke test: loader -> build -> one known image -> real RKNN inference.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
image=${1:-}
dtype=${2:-int8}
case "$dtype" in fp|int8) ;; *) printf 'Model must be fp or int8.\n' >&2; exit 2;; esac
sh "$repo/scripts/run-npu-check.sh"
sh "$repo/scripts/build-npu-vision.sh"
(cd "$repo/vision/models" && sha256sum -c SHA256SUMS)
if [ -z "$image" ]; then
    image=$(sh "$repo/scripts/fetch-npu-sample.sh")
fi
exec "$repo/build/vision/detect_image" \
    --model "$repo/vision/models/yolov5n-rk3576-$dtype.rknn" \
    --image "$image" --output "$repo/build/vision/result-$dtype.jpg" \
    --warmup 1 --repeat 3
