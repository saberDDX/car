#!/bin/sh
# Compile and validate an offline merge; never change the live tree or /boot.
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ "$#" -ne 2 ]; then
    printf 'Usage: sh scripts/build-key-overlay.sh BASE.dtb OUTPUT_DIR\n' >&2
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
    *) printf 'STOP: incompatible board device tree.\n' >&2; exit 1 ;;
esac
if fdtget -p "$base" /car-camera-key >/dev/null 2>&1 ||
   fdtget "$base" /__symbols__ car_key_pin >/dev/null 2>&1; then
    printf 'STOP: the key overlay is already present in the base tree.\n' >&2
    exit 1
fi
gpio_path=$(fdtget -t s "$base" /__symbols__ gpio4)
pinctrl_path=$(fdtget -t s "$base" /__symbols__ pinctrl)
pull_path=$(fdtget -t s "$base" /__symbols__ pcfg_pull_up)
gpio_phandle=$(fdtget -t u "$base" "$gpio_path" phandle)
pull_phandle=$(fdtget -t u "$base" "$pull_path" phandle)
require_equal "$(fdtget -t u "$base" "$gpio_path" '#gpio-cells')" 2 'GPIO cell count'
if ! fdtget -p "$base" "$pull_path" | awk '$0 == "bias-pull-up" {found=1} END {exit !found}'; then
    printf 'STOP: pcfg_pull_up does not specify bias-pull-up.\n' >&2
    exit 1
fi
mkdir -p "$output"
overlay="$output/car-lubancat3-gpio4-a6-key.dtbo"
merged="$output/merged-key.dtb"
dtc -@ -I dts -O dtb -o "$overlay" "$repo_dir/drivers/gpio-key/lubancat3-gpio4-a6-key.dts"
fdtoverlay -i "$base" -o "$merged" "$overlay"
key_path=/car-camera-key
pin_path=$(fdtget -t s "$merged" /__symbols__ car_key_pin)
require_equal "$pin_path" "$pinctrl_path/car-keys/car-key-pin" 'pinctrl node path'
pin_phandle=$(fdtget -t u "$merged" "$pin_path" phandle)
require_equal "$(fdtget -t s "$merged" "$key_path" compatible)" 'saberddx,car-gpio-key' 'driver match'
require_equal "$(fdtget -t u "$merged" "$key_path" button-gpios)" "$gpio_phandle 6 1" 'GPIO4_A6 active low'
require_equal "$(fdtget -t s "$merged" "$key_path" status)" okay 'key status'
require_equal "$(fdtget -t u "$merged" "$key_path" linux,code)" 212 'KEY_CAMERA code'
require_equal "$(fdtget -t u "$merged" "$key_path" debounce-interval)" 20 'debounce milliseconds'
require_equal "$(fdtget -t s "$merged" "$key_path" pinctrl-names)" default 'pinctrl state'
require_equal "$(fdtget -t u "$merged" "$key_path" pinctrl-0)" "$pin_phandle" 'pinctrl reference'
require_equal "$(fdtget -t u "$merged" "$pin_path" rockchip,pins)" "4 6 0 $pull_phandle" 'GPIO mux and pull-up'
printf 'PASS: overlay compiled and offline merge verified (GPIO4_A6, active low, pull-up, KEY_CAMERA, 20ms).\n'
printf 'Overlay file: %s\n' "$overlay"
