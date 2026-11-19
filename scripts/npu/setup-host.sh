#!/bin/sh
# Conversion host only (x86_64). Never run this installer on the RK3576 board.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
test "$(uname -m)" = x86_64 || { printf 'This setup is for the x86_64 conversion host.\n' >&2; exit 1; }
command -v uv >/dev/null
root=${NPU_HOST_ROOT:-$repo/build/npu-host}
mkdir -p "$root"
root=$(realpath "$root")
export UV_CACHE_DIR="$root/uv-cache"
export UV_PYTHON_INSTALL_DIR="$root/python"
if [ ! -x "$root/venv/bin/python" ]; then uv --system-certs venv --python 3.10 "$root/venv"; fi
if [ ! -d "$root/rknn-toolkit2/.git" ]; then
    git clone --depth 1 --branch v2.1.0 --filter=blob:none --sparse \
        https://github.com/airockchip/rknn-toolkit2.git "$root/rknn-toolkit2"
    git -C "$root/rknn-toolkit2" sparse-checkout set rknn-toolkit2/packages
fi
test "$(git -C "$root/rknn-toolkit2" rev-parse HEAD)" = deaba85fc437a28db0b0c29f27d8929f4c5816a1
if [ ! -d "$root/yolov5/.git" ]; then
    git clone --depth 1 --branch 'rk_opt@v6.2.1' https://github.com/airockchip/yolov5.git "$root/yolov5"
fi
test "$(git -C "$root/yolov5" rev-parse HEAD)" = 23a20ef3e5fa648e765e4a70b30edbfd3aaf8c8c
uv --system-certs pip install --python "$root/venv/bin/python" --index-url https://download.pytorch.org/whl/cpu \
    torch==2.1.0+cpu torchvision==0.16.0+cpu
uv --system-certs pip install --python "$root/venv/bin/python" -r "$repo/scripts/npu/requirements-host.txt"
uv --system-certs pip install --python "$root/venv/bin/python" --no-deps \
    "$root/rknn-toolkit2/rknn-toolkit2/packages/rknn_toolkit2-2.1.0+708089d1-cp310-cp310-linux_x86_64.whl"
uv pip check --python "$root/venv/bin/python"
if ! "$root/venv/bin/python" -c 'import onnxruntime' > "$root/ort-import.log" 2>&1; then
    if grep -q 'cannot enable executable stack' "$root/ort-import.log"; then
        # Old ORT wheel requests an executable stack. Tighten that flag for glibc 2.41.
        patch_tool=${PATCHELF:-patchelf}
        command -v "$patch_tool" >/dev/null || {
            printf 'ORT 1.16 needs patchelf with --clear-execstack on this host. Set PATCHELF; see vision/README.md.\n' >&2
            exit 1
        }
        so="$root/venv/lib/python3.10/site-packages/onnxruntime/capi/onnxruntime_pybind11_state.cpython-310-x86_64-linux-gnu.so"
        sha256sum "$so" > "$root/ort-before.sha256"
        "$patch_tool" --clear-execstack "$so"
        sha256sum "$so" > "$root/ort-after.sha256"
    else
        cat "$root/ort-import.log" >&2
        exit 1
    fi
fi
"$root/venv/bin/python" -c 'import torch, onnx, onnxruntime; from rknn.api import RKNN; print("Host imports PASS")'
printf 'Host prepared: %s\n' "$root"
