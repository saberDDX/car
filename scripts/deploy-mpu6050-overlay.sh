#!/bin/sh
# Run with sensor disconnected. Prepare, back up, and install; do not reboot.
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
output="$repo_dir/drivers/mpu6050/build"
as_root() {
    if [ "$(id -u)" -eq 0 ]; then "$@"; else sudo "$@"; fi
}
printf '[1/4] Recheck unused controller and physical pins 3/5\n'
sh "$repo_dir/scripts/prepare-mpu6050.sh"
printf '[2/4] Build the identity driver before changing boot files\n'
sh "$repo_dir/scripts/check-board-env.sh"
make -C "$repo_dir/drivers/mpu6050" -j2
printf '[3/4] Compile and verify the overlay against the running tree\n'
missing=0
for tool in dtc fdtoverlay fdtget; do
    command -v "$tool" >/dev/null 2>&1 || missing=1
done
if [ "$missing" -eq 1 ]; then
    as_root apt-get update
    as_root apt-get install -y --no-remove device-tree-compiler
fi
mkdir -p "$output"
as_root dtc -q -I fs -O dtb -o - /sys/firmware/devicetree/base > "$output/running.dtb"
sh "$repo_dir/scripts/build-mpu6050-overlay.sh" "$output/running.dtb" "$output"
printf '[4/4] Back up uEnv, install the overlay and verify both files\n'
as_root python3 "$repo_dir/scripts/install-mpu6050-overlay.py" "$output/car-lubancat3-mpu6050.dtbo"
printf 'Next: power off and unplug power before wiring. No module was loaded.\n'
