#!/bin/sh
# Prepare the fixed LubanCat-3 key overlay. No live GPIO or boot changes.
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
output="$repo_dir/drivers/gpio-key/build"
as_root() {
    if [ "$(id -u)" -eq 0 ]; then
        "$@"
    else
        sudo "$@"
    fi
}

printf '[1/4] Build driver and inspect board configuration\n'
sh "$repo_dir/scripts/prepare-gpio-key.sh"
model=$(tr -d '\000' < /sys/firmware/devicetree/base/model)
if [ "$model" != 'EmbedFire LubanCat-3' ]; then
    printf 'STOP: this overlay is only for EmbedFire LubanCat-3.\n' >&2
    exit 1
fi

printf '[2/4] Ensure device tree tools are available\n'
missing_tools=0
for tool in dtc fdtoverlay fdtget; do
    command -v "$tool" >/dev/null 2>&1 || missing_tools=1
done
if [ "$missing_tools" -eq 1 ]; then
    as_root apt-get update
    as_root apt-get install -y --no-remove device-tree-compiler
fi

printf '[3/4] Check current GPIO4_A6 ownership (physical pin 7)\n'
# Debugfs exposes kernel ownership without requesting or changing the GPIO.
if ! mountpoint -q /sys/kernel/debug; then
    as_root mount -t debugfs debugfs /sys/kernel/debug
fi
mkdir -p "$output"
as_root find /sys/kernel/debug/pinctrl -maxdepth 2 -type f -name pinmux-pins \
    -exec cat '{}' \; > "$output/pinmux-pins.txt"
awk -f "$repo_dir/scripts/check-key-pin.awk" "$output/pinmux-pins.txt"

printf '[4/4] Compile overlay and verify an offline merge\n'
# Export the current tree, including any overlays already active on this board.
# Existing base-tree dtc warnings are suppressed; overlay warnings remain visible.
as_root dtc -q -I fs -O dtb -o - /sys/firmware/devicetree/base > "$output/running.dtb"
sh "$repo_dir/scripts/build-key-overlay.sh" "$output/running.dtb" "$output"
printf 'PREPARED: no boot configuration or live GPIO was changed.\n'
printf 'Share the output before deploying the overlay and wiring the key.\n'
