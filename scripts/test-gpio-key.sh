#!/bin/sh
# Real board acceptance: one 20-pair round, then two unload/reload smoke rounds.
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
module_file="$repo_dir/drivers/gpio-key/car_gpio_key.ko"
parameter=/sys/module/car_gpio_key/parameters/run_id
key_node=/sys/firmware/devicetree/base/car-camera-key
device=/sys/bus/platform/devices/car-camera-key
driver=/sys/bus/platform/drivers/car-gpio-key
test_id="car-key-$(date +%s)-$$"
run_id="$test_id-before-load"

as_root() {
    if [ "$(id -u)" -eq 0 ]; then "$@"; else sudo "$@"; fi
}
owned_by_this_run() {
    [ -r "$parameter" ] && [ "$(cat "$parameter")" = "$run_id" ]
}
cleanup() {
    result=$?
    trap - 0 INT TERM
    if owned_by_this_run; then
        printf 'Cleaning up this test key module...\n'
        if ! as_root rmmod car_gpio_key; then
            printf 'Cleanup failed: car_gpio_key remains loaded.\n' >&2
            [ "$result" -ne 0 ] || result=1
        fi
    fi
    if [ "$result" -ne 0 ]; then
        printf 'Recent kernel diagnostics:\n' >&2
        as_root dmesg | tail -n 20 || true
    fi
    exit "$result"
}

if [ -d /sys/module/car_gpio_key ]; then
    printf 'STOP: car_gpio_key is already loaded; this test will not unload an existing instance.\n' >&2
    exit 1
fi
if [ ! -r "$key_node/compatible" ] ||
   [ "$(tr -d '\000' < "$key_node/compatible")" != saberddx,car-gpio-key ]; then
    printf 'STOP: key node is absent. Deploy the overlay, then power off/on before testing.\n' >&2
    exit 1
fi
if [ "$(tr -d '\000' < "$key_node/status")" != okay ]; then
    printf 'STOP: key node is disabled.\n' >&2
    exit 1
fi
sh "$repo_dir/scripts/check-board-env.sh"
make -C "$repo_dir/drivers/gpio-key" -j2
make -C "$repo_dir/tools/key-monitor"
[ "$(modinfo -F name "$module_file")" = car_gpio_key ] || exit 1
case "$(modinfo -F vermagic "$module_file")" in
    "$(uname -r) "*) ;;
    *) printf 'STOP: module does not match the running kernel.\n' >&2; exit 1 ;;
esac
as_root dmesg >/dev/null
trap cleanup 0
trap 'exit 130' INT
trap 'exit 143' TERM

printf 'Keep K1 released until each monitor prints its Verify prompt.\n'
for round in 1 2 3; do
    run_id="$test_id-round$round"
    printf '\n[Round %s/3] Load module and check the real platform binding\n' "$round"
    as_root insmod "$module_file" "run_id=$run_id"
    owned_by_this_run || {
        printf 'STOP: cannot confirm module ownership.\n' >&2
        exit 1
    }
    for attempt in 1 2 3 4 5 6 7 8 9 10; do
        [ "$(readlink -f "$device/driver" 2>/dev/null || true)" = "$driver" ] && break
        sleep 0.2
    done
    if [ "$(readlink -f "$device/driver" 2>/dev/null || true)" != "$driver" ] ||
       [ "$(readlink -f "$device/of_node" 2>/dev/null || true)" != "$key_node" ]; then
        printf 'STOP: module exists but car-camera-key did not bind to this driver.\n' >&2
        exit 1
    fi
    kernel_log=$(as_root dmesg)
    printf '%s\n' "$kernel_log" | awk -v marker="run_id=$run_id" 'index($0, marker)'
    count=1
    [ "$round" -ne 1 ] || count=20
    printf 'Now press/release K1 %s times; hold one press for 3 seconds.\n' "$count"
    as_root "$repo_dir/tools/key-monitor/key-monitor" --verify "$count"
    owned_by_this_run || {
        printf 'STOP: module ownership changed during testing.\n' >&2
        exit 1
    }
    as_root rmmod car_gpio_key
    if [ -d /sys/module/car_gpio_key ] || [ -L "$device/driver" ]; then
        printf 'STOP: module or binding remains after unloading.\n' >&2
        exit 1
    fi
    for event in /sys/class/input/event*/device/name; do
        [ -r "$event" ] || continue
        if [ "$(cat "$event")" = car-gpio-key ]; then
            printf 'STOP: input device remains after unloading.\n' >&2
            exit 1
        fi
    done
    printf 'Round %s PASS: real events verified; module, binding, and input device removed.\n' "$round"
done
printf '\nPASS: 20-pair key test and two reload smoke rounds passed, including long holds and no repeats.\n'
printf 'The module is unloaded; Qt camera integration is the next stage.\n'
