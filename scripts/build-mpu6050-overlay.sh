#!/bin/sh
# Validate a merge against the running base; never write /boot or live pins.
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ "$#" -ne 2 ]; then
    printf 'Usage: sh scripts/build-mpu6050-overlay.sh BASE.dtb OUTPUT_DIR\n' >&2
    exit 2
fi
base=$1
output=$2
for tool in dtc fdtoverlay fdtget; do
    command -v "$tool" >/dev/null 2>&1 || {
        printf 'Missing tool: %s (package: device-tree-compiler)\n' "$tool" >&2
        exit 1
    }
done
require_equal() {
    if [ "$1" != "$2" ]; then
        printf 'STOP: %s: expected [%s], got [%s]\n' "$3" "$2" "$1" >&2
        exit 1
    fi
}
require_equal "$(fdtget -t s "$base" / model)" 'EmbedFire LubanCat-3' 'board model'
case " $(fdtget -t s "$base" / compatible) " in
    *' embedfire,rk3576-lubancat-3 '*) ;;
    *) printf 'STOP: incompatible board.\n' >&2; exit 1 ;;
esac
controller=$(fdtget -t s "$base" /__symbols__ i2c7)
require_equal "$controller" /i2c@2aca0000 'I2C7 controller'
require_equal "$(fdtget -t s "$base" "$controller" status)" disabled 'unused I2C7'
require_equal "$(fdtget -t u "$base" "$controller" '#address-cells')" 1 'I2C address cells'
require_equal "$(fdtget -t u "$base" "$controller" '#size-cells')" 0 'I2C size cells'
for child in $(fdtget -l "$base" "$controller"); do
    address=$(fdtget -t u "$base" "$controller/$child" reg 2>/dev/null || true)
    if [ "$address" = 104 ] || [ "$child" = mpu6050@68 ]; then
        printf 'STOP: I2C7 already has a device at 0x68.\n' >&2
        exit 1
    fi
done
pin=$(fdtget -t s "$base" /__symbols__ i2c7m1_xfer)
pull=$(fdtget -t s "$base" /__symbols__ pcfg_pull_none_smt)
pin_phandle=$(fdtget -t u "$base" "$pin" phandle)
pull_phandle=$(fdtget -t u "$base" "$pull" phandle)
require_equal "$(fdtget -t u "$base" "$pin" rockchip,pins)" \
    "3 0 11 $pull_phandle 3 1 11 $pull_phandle" 'I2C7 M1 physical 5/3'
for property in bias-disable input-schmitt-enable; do
    if ! fdtget -p "$base" "$pull" | awk -v wanted="$property" '$0 == wanted {found=1} END {exit !found}'; then
        printf 'STOP: pin configuration lacks %s.\n' "$property" >&2
        exit 1
    fi
done
mkdir -p "$output"
overlay="$output/car-lubancat3-mpu6050.dtbo"
merged="$output/merged-mpu6050.dtb"
dtc -@ -I dts -O dtb -o "$overlay" "$repo_dir/drivers/mpu6050/lubancat3-mpu6050.dts"
fdtoverlay -i "$base" -o "$merged" "$overlay"
require_equal "$(fdtget -t s "$merged" "$controller" status)" okay 'I2C7 enabled'
require_equal "$(fdtget -t u "$merged" "$controller" clock-frequency)" 100000 'I2C clock'
require_equal "$(fdtget -t s "$merged" "$controller" pinctrl-names)" default 'pinctrl state'
require_equal "$(fdtget -t u "$merged" "$controller" pinctrl-0)" "$pin_phandle" 'M1 pin group'
node="$controller/mpu6050@68"
require_equal "$(fdtget -t s "$merged" "$node" compatible)" saberddx,car-mpu6050 'driver match'
require_equal "$(fdtget -t u "$merged" "$node" reg)" 104 '7-bit address 0x68'
require_equal "$(fdtget -t s "$merged" "$node" status)" okay 'sensor enabled'
printf 'PASS: offline merge verified (I2C7 M1, 100kHz, address 0x68).\n'
printf 'Overlay file: %s\n' "$overlay"
