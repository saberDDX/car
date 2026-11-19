#!/bin/sh
# stdout contains only the verified sample path, for use by other scripts.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
image="$repo/build/vision/bus.jpg"
expected=fb4914d123d97c440cd127ef0e98d4bdc68cd88e2683657528928b4a34014e16
mkdir -p "$repo/build/vision"
if [ ! -f "$image" ]; then
    curl -fL --connect-timeout 15 --max-time 90 \
      https://raw.githubusercontent.com/airockchip/rknn_model_zoo/c2b7d00714b4e5d21266ab3003f3ca687ba0d57b/examples/yolov5/model/bus.jpg \
      -o "$image.tmp"
    printf '%s  %s\n' "$expected" "$image.tmp" | sha256sum -c - >&2
    mv "$image.tmp" "$image"
fi
printf '%s  %s\n' "$expected" "$image" | sha256sum -c - >&2
printf '%s\n' "$image"
