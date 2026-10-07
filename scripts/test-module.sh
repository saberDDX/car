#!/bin/sh
# Run on the board. Only load/unload this project's baseline module.
set -eu

repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
module_dir="$repo_dir/drivers/hello"
module_file="$module_dir/car_hello.ko"
run_id="car-$(date +%s)-$$"
parameter_file=/sys/module/car_hello/parameters/run_id

as_root()
{
    if [ "$(id -u)" -eq 0 ]; then
        "$@"
    else
        sudo "$@"
    fi
}

owned_by_this_run()
{
    [ -r "$parameter_file" ] && [ "$(cat "$parameter_file")" = "$run_id" ]
}

cleanup()
{
    result=$?
    trap - 0 INT TERM
    if owned_by_this_run; then
        printf 'Cleaning up this test module...\n'
        if ! as_root rmmod car_hello; then
            printf 'Cleanup failed: car_hello remains loaded.\n' >&2
            [ "$result" -ne 0 ] || result=1
        fi
    fi
    exit "$result"
}

trap cleanup 0
trap 'exit 130' INT
trap 'exit 143' TERM

if [ -d /sys/module/car_hello ]; then
    printf 'car_hello is already loaded; this test will not unload an existing module.\n' >&2
    exit 1
fi

printf '[1/4] Check board prerequisites\n'
sh "$repo_dir/scripts/check-board-env.sh"

printf '[2/4] Build baseline module\n'
make -C "$module_dir" -j2
module_name=$(modinfo -F name "$module_file")
vermagic=$(modinfo -F vermagic "$module_file")
kernel_release=$(uname -r)
if [ "$module_name" != car_hello ]; then
    printf 'Unexpected module name.\n' >&2
    exit 1
fi
case "$vermagic" in
    "$kernel_release "*) printf 'vermagic: %s\n' "$vermagic" ;;
    *) printf 'Module kernel version does not match the running kernel.\n' >&2; exit 1 ;;
esac

# Check log access before loading anything. sudo may request a password.
as_root dmesg >/dev/null

printf '[3/4] Load module and verify this run\n'
if ! as_root insmod "$module_file" "run_id=$run_id"; then
    printf 'Module loading failed.\n' >&2
    kernel_log=$(as_root dmesg) || exit 1
    printf '%s\n' "$kernel_log" | tail -n 20
    exit 1
fi
if ! owned_by_this_run; then
    printf 'Cannot confirm that the loaded module belongs to this run.\n' >&2
    exit 1
fi
kernel_log=$(as_root dmesg)
case "$kernel_log" in
    *"car_hello: loaded (driver baseline) run_id=$run_id"*) ;;
    *) printf 'Load log for this run was not found.\n' >&2; exit 1 ;;
esac

printf '[4/4] Unload module and verify this run\n'
as_root rmmod car_hello
if [ -d /sys/module/car_hello ]; then
    printf 'car_hello is still present after unloading.\n' >&2
    exit 1
fi
kernel_log=$(as_root dmesg)
case "$kernel_log" in
    *"car_hello: unloaded run_id=$run_id"*) ;;
    *) printf 'Unload log for this run was not found.\n' >&2; exit 1 ;;
esac
printf '%s\n' "$kernel_log" | awk -v marker="run_id=$run_id" 'index($0, marker)'
printf 'PASS: module compiled, loaded, and unloaded; both logs verified for this run.\n'
