#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
sh "$repo/scripts/build-npu-vision.sh"
(cd "$repo/vision/models" && sha256sum -c SHA256SUMS)
image=${1:-}
if [ -z "$image" ]; then image=$(sh "$repo/scripts/fetch-npu-sample.sh"); fi
exec python3 "$repo/scripts/npu/compare_models.py" --binary "$repo/build/vision/detect_image" \
    --fp "$repo/vision/models/yolov5n-rk3576-fp.rknn" --int8 "$repo/vision/models/yolov5n-rk3576-int8.rknn" \
    --image "$image" --results "$repo/build/vision/comparisons" --warmup 10 --repeat 100
