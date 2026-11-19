#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
sdk="$repo/build/rknn-sdk/include"
revision=deaba85fc437a28db0b0c29f27d8929f4c5816a1
header_sha=eeb7260a8439e7c8894a704502ea640878c92aa956fafac18136b0784e2228cb
mkdir -p "$sdk"
# Keep the upstream proprietary SDK header external to committed application code.
if [ -z "${RKNN_INCLUDE_DIR:-}" ]; then
    if [ ! -f "$sdk/rknn_api.h" ]; then
        curl -fL --connect-timeout 15 --max-time 90 \
            "https://raw.githubusercontent.com/airockchip/rknn-toolkit2/$revision/rknpu2/runtime/Linux/librknn_api/include/rknn_api.h" \
            -o "$sdk/rknn_api.h.tmp"
        printf '%s  %s\n' "$header_sha" "$sdk/rknn_api.h.tmp" | sha256sum -c -
        mv "$sdk/rknn_api.h.tmp" "$sdk/rknn_api.h"
    fi
    printf '%s  %s\n' "$header_sha" "$sdk/rknn_api.h" | sha256sum -c -
    RKNN_INCLUDE_DIR="$sdk"
fi
cmake -S "$repo/vision" -B "$repo/build/vision" -DRKNN_INCLUDE_DIR="$RKNN_INCLUDE_DIR"
cmake --build "$repo/build/vision" --parallel 2
ctest --test-dir "$repo/build/vision" --output-on-failure
