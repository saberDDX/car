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
    image="$repo/build/vision/bus.jpg"
    expected=fb4914d123d97c440cd127ef0e98d4bdc68cd88e2683657528928b4a34014e16
    if [ ! -f "$image" ]; then
        curl -fL --connect-timeout 15 --max-time 90 \
          https://raw.githubusercontent.com/airockchip/rknn_model_zoo/c2b7d00714b4e5d21266ab3003f3ca687ba0d57b/examples/yolov5/model/bus.jpg \
          -o "$image.tmp"
        printf '%s  %s\n' "$expected" "$image.tmp" | sha256sum -c -
        mv "$image.tmp" "$image"
    fi
    printf '%s  %s\n' "$expected" "$image" | sha256sum -c -
fi
exec "$repo/build/vision/detect_image" \
    --model "$repo/vision/models/yolov5n-rk3576-$dtype.rknn" \
    --image "$image" --output "$repo/build/vision/result-$dtype.jpg" \
    --warmup 1 --repeat 3
