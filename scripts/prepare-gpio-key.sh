#!/bin/sh
# Build only; pin selection and device tree deployment follow wiring review.
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

printf '[1/3] Check prerequisites\n'
sh "$repo_dir/scripts/check-board-env.sh"

printf '[2/3] Build GPIO key module and event monitor\n'
make -C "$repo_dir/drivers/gpio-key" -j2
make -C "$repo_dir/tools/key-monitor"
modinfo "$repo_dir/drivers/gpio-key/car_gpio_key.ko"

printf '[3/3] Read board identity and overlay configuration\n'
for property in model compatible; do
    path="/sys/firmware/devicetree/base/$property"
    if [ -r "$path" ]; then
        printf '%s:\n' "$property"
        tr '\000' '\n' < "$path"
        printf '\n'
    fi
done
for boot_file in /boot/uEnv/uEnv.txt /boot/uEnv.txt /boot/extlinux/extlinux.conf; do
    if [ -r "$boot_file" ]; then
        printf 'Boot configuration fields in %s:\n' "$boot_file"
        if command -v readlink >/dev/null 2>&1; then
            readlink -f "$boot_file"
        fi
        awk '/^[[:space:]]*(uname_r|dtb|dtbo|dtoverlay|enable_uboot_overlays|fdtfile|overlays)[[:space:]]*=/ || /^[[:space:]]*FDT(OVERLAYS|DIR)?[[:space:]]/ {print}' "$boot_file"
    fi
done
for tool in dtc fdtoverlay fdtget; do
    if command -v "$tool" >/dev/null 2>&1; then
        printf '%s: found\n' "$tool"
    else
        printf '%s: not installed\n' "$tool"
    fi
done
for symbol in gpio0 gpio1 gpio2 gpio3 gpio4 pinctrl pcfg_pull_up; do
    path="/sys/firmware/devicetree/base/__symbols__/$symbol"
    if [ -r "$path" ]; then
        printf 'Device tree symbol %s: ' "$symbol"
        tr '\000' '\n' < "$path"
        printf '\n'
    fi
done
printf 'Build complete. Next: confirm wiring, prepare device tree, and test real input events.\n'
