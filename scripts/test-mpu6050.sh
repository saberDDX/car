#!/bin/sh
# Stage 1 real-board test: WHO_AM_I reads and three binding/unbinding rounds.
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
module="$repo_dir/drivers/mpu6050/car_mpu6050.ko"
parameter=/sys/module/car_mpu6050/parameters/run_id
node=/sys/firmware/devicetree/base/i2c@2aca0000/mpu6050@68
driver=/sys/bus/i2c/drivers/car-mpu6050
test_id="car-mpu-$(date +%s)-$$"
run_id="$test_id-before-load"
as_root() {
    if [ "$(id -u)" -eq 0 ]; then "$@"; else sudo "$@"; fi
}
owned() {
    [ -r "$parameter" ] && [ "$(cat "$parameter")" = "$run_id" ]
}
cleanup() {
    result=$?
    trap - 0 INT TERM
    if owned; then
        if ! as_root rmmod car_mpu6050; then
            printf 'STOP: cannot clean up this test module.\n' >&2
            [ "$result" -ne 0 ] || result=1
        fi
    fi
    if [ "$result" -ne 0 ]; then
        as_root dmesg | tail -n 25 || true
    fi
    exit "$result"
}
[ ! -d /sys/module/car_mpu6050 ] || {
    printf 'STOP: car_mpu6050 is already loaded; an existing instance will not be unloaded.\n' >&2
    exit 1
}
if [ ! -r "$node/compatible" ] ||
   [ "$(tr -d '\000' < "$node/compatible")" != saberddx,car-mpu6050 ] ||
   [ "$(tr -d '\000' < "$node/status")" != okay ]; then
    printf 'STOP: MPU6050 node is absent/disabled. Deploy the overlay and power off/on first.\n' >&2
    exit 1
fi
device=
for candidate in /sys/bus/i2c/devices/*-0068; do
    [ "$(readlink -f "$candidate/of_node" 2>/dev/null || true)" = "$node" ] || continue
    [ -z "$device" ] || { printf 'STOP: ambiguous client selection.\n' >&2; exit 1; }
    device=$candidate
done
[ -n "$device" ] || { printf 'STOP: the I2C client was not created.\n' >&2; exit 1; }
if [ -L "$device/driver" ]; then
    printf 'STOP: the client is already bound to %s; leaving it unchanged.\n' "$(readlink -f "$device/driver")" >&2
    exit 1
fi
sh "$repo_dir/scripts/check-board-env.sh"
make -C "$repo_dir/drivers/mpu6050" -j2
[ "$(modinfo -F name "$module")" = car_mpu6050 ] || exit 1
case "$(modinfo -F vermagic "$module")" in
    "$(uname -r) "*) ;;
    *) printf 'STOP: module does not match the running kernel.\n' >&2; exit 1 ;;
esac
as_root dmesg >/dev/null
trap cleanup 0
trap 'exit 130' INT
trap 'exit 143' TERM
for round in 1 2 3; do
    run_id="$test_id-round$round"
    printf '\n[Round %s/3] Load, check binding and read the real identity register\n' "$round"
    as_root insmod "$module" "run_id=$run_id"
    owned || { printf 'STOP: module ownership cannot be confirmed.\n' >&2; exit 1; }
    if [ "$(readlink -f "$device/driver" 2>/dev/null || true)" != "$driver" ]; then
        printf 'STOP: module loaded but probe did not bind. Check power, SDA/SCL and AD0.\n' >&2
        exit 1
    fi
    kernel_log=$(as_root dmesg)
    printf '%s\n' "$kernel_log" | awk -v marker="run_id=$run_id" 'index($0, marker)'
    case "$kernel_log" in
        *"WHO_AM_I=0x68 address=0x68 run_id=$run_id"*) ;;
        *) printf 'STOP: no identity success log for this run.\n' >&2; exit 1 ;;
    esac
    for sample in 1 2 3 4 5; do
        identity=$(cat "$device/who_am_i")
        printf 'Read %s: WHO_AM_I=%s\n' "$sample" "$identity"
        [ "$identity" = 0x68 ] || { printf 'STOP: unexpected identity.\n' >&2; exit 1; }
        sleep 0.05
    done
    owned || { printf 'STOP: module ownership changed.\n' >&2; exit 1; }
    as_root rmmod car_mpu6050
    if [ -d /sys/module/car_mpu6050 ] || [ -L "$device/driver" ] || [ -e "$device/who_am_i" ]; then
        printf 'STOP: module, binding or diagnostic attribute remains after unload.\n' >&2
        exit 1
    fi
    printf 'Round %s PASS: identity and cleanup verified.\n' "$round"
done
printf '\nPASS: MPU6050 identity read 15 times and three load/unload rounds passed.\n'
printf 'The module is unloaded. Six-axis configuration/data capture is the next stage.\n'
