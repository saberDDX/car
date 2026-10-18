#!/bin/sh
# Board test: identity by default; --data [seconds] adds six-axis/ABI tests.
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
module="$repo_dir/drivers/mpu6050/car_mpu6050.ko"
parameter=/sys/module/car_mpu6050/parameters/run_id
node=/sys/firmware/devicetree/base/i2c@2aca0000/mpu6050@68
driver=/sys/bus/i2c/drivers/car-mpu6050
test_id="car-mpu-$(date +%s)-$$"
run_id="$test_id-before-load"
data_seconds=
case "${1:-}" in
    '') ;;
    --data)
        [ "$#" -le 2 ] || { printf 'Usage: %s [--data seconds]\n' "$0" >&2; exit 1; }
        data_seconds=${2:-10}
        python3 - "$data_seconds" <<'PY'
import sys
value = int(sys.argv[1])
if not 1 <= value <= 3600:
    raise SystemExit("seconds must be an integer in 1..3600")
PY
        ;;
    *) printf 'Usage: %s [--data seconds]\n' "$0" >&2; exit 1 ;;
esac
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
sh "$repo_dir/scripts/check-build-clock.sh"
if [ -n "$data_seconds" ]; then
    # A clean rebuild avoids reusing products made before correcting board time.
    make -C "$repo_dir/drivers/mpu6050" clean
    make -C "$repo_dir/tools/mpu6050-monitor" clean
    make -C "$repo_dir/tools/mpu6050-monitor"
    log_dir="$repo_dir/build/mpu6050/$test_id"
    mkdir -p "$log_dir"
fi
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
    if [ -n "$data_seconds" ]; then
        [ "$(readlink -f /sys/class/misc/car_mpu6050/device)" = "$(readlink -f "$device")" ] || {
            printf 'STOP: misc device belongs to an unexpected client.\n' >&2; exit 1;
        }
        wait_count=0
        while [ ! -c /dev/car_mpu6050 ] && [ "$wait_count" -lt 20 ]; do
            sleep 0.05
            wait_count=$((wait_count + 1))
        done
        [ -c /dev/car_mpu6050 ] || { printf 'STOP: misc device node missing.\n' >&2; exit 1; }
        seconds=1
        if [ "$round" -eq 1 ]; then
            seconds=$data_seconds
            printf 'Keep the sensor still for the first 3 seconds, then gently tilt/rotate it. Do not unplug wires.\n'
        fi
        as_root "$repo_dir/tools/mpu6050-monitor/mpu6050-monitor" \
            -s "$seconds" -r 20 -o "$log_dir/round$round.csv"
        if [ "$round" -eq 3 ]; then
            owned || exit 1
            as_root python3 "$repo_dir/scripts/check-mpu6050-abi.py" \
                --client "${device##*/}" --run-id "$run_id"
        fi
    fi
    owned || { printf 'STOP: module ownership changed.\n' >&2; exit 1; }
    as_root rmmod car_mpu6050
    if [ -d /sys/module/car_mpu6050 ] || [ -L "$device/driver" ] || [ -e "$device/who_am_i" ] ||
       [ -e /sys/class/misc/car_mpu6050 ]; then
        printf 'STOP: module, binding or diagnostic attribute remains after unload.\n' >&2
        exit 1
    fi
    printf 'Round %s PASS: identity and cleanup verified.\n' "$round"
done
printf '\nPASS: MPU6050 identity read 15 times and three load/unload rounds passed.\n'
if [ -n "$data_seconds" ]; then
    printf 'PASS: six-axis reads, ABI/concurrent reads and open-fd unbind/rebind passed.\n'
    printf 'CSV logs: %s\n' "$log_dir"
    printf 'Module unloaded. Review the readings and measured duration above; PASS covers the automated checks.\n'
else
    printf 'The module is unloaded. Run sh scripts/test-mpu6050-data.sh for six-axis tests.\n'
fi
