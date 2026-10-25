#!/bin/sh
# Build artifacts only. This script never installs or changes board boot files.
set -eu
[ "$#" -eq 4 ] || { printf 'Usage: %s KERNEL_SOURCE BOARD_CONFIG OUTPUT CROSS_PREFIX\n' "$0" >&2; exit 1; }
source_dir=$(realpath "$1")
config=$(realpath "$2")
source_commit=$(git -C "$source_dir" rev-parse HEAD)
[ "$source_commit" = b73c060dbe3576f056c197d7f2f7cc11f65e2aec ] || {
    printf 'STOP: this recipe is pinned to vendor source b73c060; review a different revision first.\n' >&2
    exit 1
}
mkdir -p "$3"
output=$(realpath "$3")
cross=$4
[ "$source_dir" != "$output" ] || { printf 'STOP: use a separate output directory.\n' >&2; exit 1; }
[ -f "$source_dir/drivers/misc/car_mpu6050/Kconfig" ] || { printf 'STOP: apply and review the integration patch first.\n' >&2; exit 1; }
[ ! -e "$output/.config" ] || { printf 'STOP: output already configured; preserve it and use a new directory.\n' >&2; exit 1; }
compiler=${BSP_CC:-${cross}gcc}
command -v "$compiler" >/dev/null
for tool in make gcc bison flex bc; do command -v "$tool" >/dev/null || { printf 'Missing tool: %s\n' "$tool" >&2; exit 1; }; done
kmake() {
    make -C "$source_dir" O="$output" ARCH=arm64 CROSS_COMPILE="$cross" CC="$compiler" LOCALVERSION= \
        HOSTCFLAGS="${BSP_HOSTCFLAGS:--O2}" HOSTLDFLAGS="${BSP_HOSTLDFLAGS:-}" "$@"
}
cp "$config" "$output/.config"
cp "$config" "$output/board-original.config"
# GCC plugins were enabled without active plugins in the original board config.
# Keep that build-time difference explicit for the portable cross-toolchain.
"$source_dir/scripts/config" --file "$output/.config" \
    --set-str LOCALVERSION '-rk3576-car-bsp' --disable LOCALVERSION_AUTO \
    --disable GCC_PLUGINS --module CAR_MPU6050
{
    git -C "$source_dir" rev-parse HEAD
    "$compiler" --version
    sha256sum "$config"
    printf 'Integration and source modifications:\n'
    git -C "$source_dir" diff --stat -- drivers/misc/Kconfig drivers/misc/Makefile
    sha256sum "$source_dir"/drivers/misc/car_mpu6050/*
} > "$output/build-inputs.txt"
kmake olddefconfig
"$source_dir/scripts/diffconfig" "$output/board-original.config" "$output/.config" > "$output/config-diff.txt"
release=$(kmake -s kernelrelease)
[ "$release" = '6.1.99-rk3576-car-bsp' ] || { printf 'STOP: unexpected kernelrelease: %s\n' "$release" >&2; exit 1; }
kmake -j"${BSP_JOBS:-2}" Image modules rockchip/rk3576-lubancat-3.dtb
printf '\nBUILT: %s (not installed or boot-tested)\n' "$release"
printf 'Review %s/config-diff.txt and build-inputs.txt\n' "$output"
