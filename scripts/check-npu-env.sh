#!/bin/sh
# Read-only M0 inventory. Run on the board; never installs or changes devices.
# Missing tools/permissions are observations, not proof that NPU is unavailable.
set -u

printf '\n[1] Running system\n'
date -u '+UTC: %Y-%m-%dT%H:%M:%SZ'
uname -a
if [ -r /proc/device-tree/model ]; then
    tr '\000' '\n' < /proc/device-tree/model
fi
if [ -r /etc/os-release ]; then
    grep -E '^(PRETTY_NAME|VERSION_ID)=' /etc/os-release
fi
getconf GNU_LIBC_VERSION 2>/dev/null || true

printf '\n[2] Running-kernel configuration\n'
if [ -r /proc/config.gz ]; then
    zcat /proc/config.gz | grep -E '^(CONFIG_ROCKCHIP_RKNPU|# CONFIG_ROCKCHIP_RKNPU)' || true
elif [ -r "/boot/config-$(uname -r)" ]; then
    grep -E '^(CONFIG_ROCKCHIP_RKNPU|# CONFIG_ROCKCHIP_RKNPU)' "/boot/config-$(uname -r)" || true
else
    printf 'Running-kernel config unavailable at standard paths.\n'
fi

printf '\n[3] Device nodes and actual driver binding\n'
id
ls -l /dev/dri/renderD* /dev/rknpu* 2>/dev/null || true
for node in /sys/class/drm/renderD*; do
    [ -e "$node" ] || continue
    printf '%s\n' "$node"
    readlink -f "$node/device/driver" || true
    if [ -r "$node/device/uevent" ]; then
        grep -E '^(DRIVER|OF_NAME|OF_COMPATIBLE_[0-9]+)=' "$node/device/uevent" || true
    fi
done
for path in /sys/module/rknpu/version /sys/kernel/debug/rknpu/version; do
    if [ -r "$path" ]; then
        printf '%s: ' "$path"
        cat "$path"
    else
        printf '%s: absent or unreadable\n' "$path"
    fi
done

printf '\n[4] RKNN Runtime files (metadata only; not loaded)\n'
for lib in /lib/librknnrt.so /usr/lib/librknnrt.so \
           /usr/lib/aarch64-linux-gnu/librknnrt.so /usr/local/lib/librknnrt.so; do
    [ -e "$lib" ] || continue
    readlink -f "$lib"
    file "$lib" 2>/dev/null || true
    sha256sum "$lib" 2>/dev/null || true
    if command -v readelf >/dev/null 2>&1; then
        readelf -d "$lib" 2>/dev/null | grep -E 'NEEDED|SONAME' || true
    fi
    if command -v strings >/dev/null 2>&1; then
        strings "$lib" | grep -E -m 6 'librknnrt version|rknnrt version|RKNN Runtime' || true
    fi
done

printf '\n[5] Board build tools\n'
for tool in gcc g++ cmake pkg-config python3; do
    command -v "$tool" || true
done
pkg-config --modversion opencv4 2>/dev/null || true
printf '\nInventory only: actual SDK/driver query and model inference remain required.\n'
