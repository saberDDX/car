#!/bin/sh
# Reuse installed experiment modules; compile only native userspace monitors.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
release=6.1.99-rk3576-car-bsp
run_id="car-bsp-runtime-$(date +%s)-$$"
log_dir="$repo/build/bsp/$run_id"

as_root() {
    if [ "$(id -u)" -eq 0 ]; then "$@"; else sudo "$@"; fi
}
owned() {
    parameter="/sys/module/$1/parameters/run_id"
    [ -r "$parameter" ] && [ "$(cat "$parameter")" = "$run_id" ]
}
cleanup() {
    result=$?
    trap - 0 INT TERM
    for module in car_mpu6050 car_gpio_key; do
        if owned "$module"; then
            if ! as_root rmmod "$module"; then
                printf 'Cleanup failed for this run\047s %s.\n' "$module" >&2
                [ "$result" -ne 0 ] || result=1
            fi
        fi
    done
    if [ "$result" -ne 0 ]; then
        as_root dmesg | tail -n 30 || true
    fi
    exit "$result"
}

[ "$(uname -r)" = "$release" ] || {
    printf 'STOP: boot into %s first.\n' "$release" >&2; exit 1;
}
for tool in make cc python3 modinfo modprobe timeout; do
    command -v "$tool" >/dev/null 2>&1 || {
        printf 'STOP: missing %s.\n' "$tool" >&2; exit 1;
    }
done
printf '[1/4] Check installed modules and build native userspace monitors\n'
for module in car_gpio_key car_mpu6050; do
    case "$(modinfo -k "$release" -F vermagic "$module")" in
        "$release "*) ;;
        *) printf 'STOP: mismatched installed module %s.\n' "$module" >&2; exit 1 ;;
    esac
done
make -B -C "$repo/tools/key-monitor"
make -B -C "$repo/tools/mpu6050-monitor"
mkdir -p "$log_dir"
trap cleanup 0
trap 'exit 130' INT
trap 'exit 143' TERM

printf '[2/4] Load or reuse installed modules; verify live configuration/tree/bindings\n'
for module in car_gpio_key car_mpu6050; do
    if [ ! -d "/sys/module/$module" ]; then
        as_root modprobe "$module" "run_id=$run_id"
    else
        printf 'Reusing already loaded %s; it will not be unloaded by this run.\n' "$module"
    fi
done
# Device nodes created by udev may appear slightly after probe returns.
for attempt in 1 2 3 4 5 6 7 8 9 10; do
    [ ! -c /dev/car_mpu6050 ] || break
    sleep 0.2
done
python3 "$repo/scripts/check-bsp-runtime.py" --output "$log_dir/kernel-tree.json"

printf '[3/4] Sample MPU6050 for 10 seconds at a target 20Hz\n'
printf 'Keep still for 3 seconds, then gently tilt/rotate; leave all wires connected.\n'
as_root "$repo/tools/mpu6050-monitor/mpu6050-monitor" -s 10 -r 20 -o "$log_dir/mpu6050.csv"

printf '[4/4] Verify 3 K1 press/release pairs; hold one press for 3 seconds\n'
printf 'Release K1 first. Start pressing only after the monitor prints Verify.\n'
as_root timeout --foreground 90 "$repo/tools/key-monitor/key-monitor" --verify 3
printf '\nPASS: new-kernel configuration, tree, bindings, six-axis reads and physical key events.\n'
printf 'Reports: %s\n' "$log_dir"
printf 'Camera enumeration is in kernel-tree.json; Qt live video still needs a visual check.\n'
printf 'Only modules loaded with this run_id are cleaned up; pre-existing modules are retained.\n'
