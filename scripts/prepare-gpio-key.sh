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
for boot_file in /boot/uEnv/uEnv*.txt /boot/uEnv.txt /boot/extlinux/extlinux.conf; do
    if [ -r "$boot_file" ]; then
        printf 'Boot selection in %s:\n' "$boot_file"
        awk '/^(uname_r|dtb|dtbo|fdtfile|overlays)=/ || /^[[:space:]]*FDT(OVERLAYS|DIR)?[[:space:]]/ {print}' "$boot_file"
    fi
done
printf 'Build complete. Next: confirm wiring, prepare device tree, and test real input events.\n'
