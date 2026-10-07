#!/bin/sh
# Preflight only: no I2C transactions, driver loading, or boot configuration edits.
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
output="$repo_dir/build/mpu6050"
as_root() {
    if [ "$(id -u)" -eq 0 ]; then "$@"; else sudo "$@"; fi
}

printf '[1/3] Board and kernel support\n'
model=$(tr -d '\000' < /sys/firmware/devicetree/base/model)
printf 'Model: %s\n' "$model"
[ "$model" = 'EmbedFire LubanCat-3' ] || { printf 'STOP: unexpected board.\n' >&2; exit 1; }
printf 'Kernel: %s\n' "$(uname -r)"
config="/lib/modules/$(uname -r)/build/include/config/auto.conf"
[ -r "$config" ] || { printf 'STOP: kernel build configuration is unavailable.\n' >&2; exit 1; }
awk '/^CONFIG_(I2C|I2C_RK3X|I2C_CHARDEV|IIO|INV_MPU6050_I2C|INV_MPU6050_SPI)=/ {print}' "$config"

printf '[2/3] Current I2C7 device tree and adapters\n'
symbols=/sys/firmware/devicetree/base/__symbols__
for symbol in i2c7 i2c7m1_xfer pcfg_pull_none_smt; do
    if [ -r "$symbols/$symbol" ]; then
        printf '%s: ' "$symbol"
        tr '\000' '\n' < "$symbols/$symbol"
        printf '\n'
    else
        printf '%s: symbol not present\n' "$symbol"
    fi
done
[ -r "$symbols/i2c7" ] || { printf 'STOP: no I2C7 symbol in the running tree.\n' >&2; exit 1; }
controller=$(tr -d '\000' < "$symbols/i2c7")
case "$controller" in
    /i2c@2aca0000) ;;
    *) printf 'STOP: I2C7 path differs from the verified RK3576 controller.\n' >&2; exit 1 ;;
esac
status=okay
if [ -r "/sys/firmware/devicetree/base$controller/status" ]; then
    status=$(tr -d '\000' < "/sys/firmware/devicetree/base$controller/status")
fi
printf 'I2C7 controller status: %s\n' "$status"
for adapter in /sys/class/i2c-adapter/i2c-*; do
    [ -r "$adapter/name" ] || continue
    printf '%s: %s\n' "${adapter##*/}" "$(cat "$adapter/name")"
done
boot=/boot/uEnv/uEnv.txt
if [ -r "$boot" ]; then
    printf 'Active uEnv: %s\n' "$(readlink -f "$boot")"
    awk '/^[[:space:]]*(uname_r|enable_uboot_overlays|dtoverlay)[[:space:]]*=/ {print}' "$boot"
fi
if [ "$status" != disabled ]; then
    printf 'STOP: I2C7 is already enabled; inspect its clients and current pin group before changing it.\n' >&2
    exit 1
fi

printf '[3/3] Check proposed I2C7 M1 pins, without requesting them\n'
if ! mountpoint -q /sys/kernel/debug; then
    as_root mount -t debugfs debugfs /sys/kernel/debug
fi
mkdir -p "$output"
as_root find /sys/kernel/debug/pinctrl -maxdepth 2 -type f -name pinmux-pins \
    -exec cat '{}' \; > "$output/pinmux-pins.txt"
awk -f "$repo_dir/scripts/check-i2c7-pins.awk" "$output/pinmux-pins.txt"
printf 'PREPARED: I2C7 is disabled and physical pins 3/5 are unclaimed.\n'
printf 'No sensor transactions, boot configuration edits, or driver loads were performed.\n'
printf 'Next: confirm the MPU6050 module pin labels and supply, then prepare its driver/overlay.\n'
