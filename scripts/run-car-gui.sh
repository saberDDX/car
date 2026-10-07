#!/bin/sh
# Run from a terminal inside the board's VNC desktop. GUI stays unprivileged.
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir="$repo_dir/build/qt"
module_file="$repo_dir/drivers/gpio-key/car_gpio_key.ko"
parameter=/sys/module/car_gpio_key/parameters/run_id
run_id="car-gui-$(date +%s)-$$"
acl_dir=
key_device=
video_device=
key_inode=
video_inode=

as_root() {
    if [ "$(id -u)" -eq 0 ]; then "$@"; else sudo "$@"; fi
}
owned_by_this_run() {
    [ -r "$parameter" ] && [ "$(cat "$parameter")" = "$run_id" ]
}
restore_acl() {
    device=$1
    inode=$2
    backup=$3
    if [ -n "$device" ] && [ -f "$backup" ] &&
       [ "$(stat -Lc '%d:%i:%t:%T' "$device" 2>/dev/null || true)" = "$inode" ]; then
        as_root setfacl --restore="$backup"
    fi
}
cleanup() {
    result=$?
    trap - 0 INT TERM
    if [ -n "$acl_dir" ]; then
        acl_restore_failed=0
        if ! restore_acl "$key_device" "$key_inode" "$acl_dir/key.acl"; then
            printf 'Could not restore key permissions.\n' >&2
            result=1
            acl_restore_failed=1
        fi
        if ! restore_acl "$video_device" "$video_inode" "$acl_dir/video.acl"; then
            printf 'Could not restore camera permissions.\n' >&2
            result=1
            acl_restore_failed=1
        fi
        if [ "$acl_restore_failed" -eq 0 ]; then
            rm -rf -- "$acl_dir"
        else
            printf 'Original permission backups retained in %s\n' "$acl_dir" >&2
        fi
    fi
    if owned_by_this_run; then
        if ! as_root rmmod car_gpio_key; then
            printf 'Could not unload this run\047s key module.\n' >&2
            result=1
        fi
    fi
    exit "$result"
}

if [ "$(id -u)" -eq 0 ]; then
    printf 'Run this script as cat, not sudo; it requests sudo only for driver/device setup.\n' >&2
    exit 1
fi
if [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ]; then
    printf 'Open a terminal inside the board\047s VNC desktop and run this command there.\n' >&2
    exit 1
fi
if [ ! -r /sys/firmware/devicetree/base/car-camera-key/compatible ]; then
    printf 'Key overlay is not active. Deploy it and reboot before launching.\n' >&2
    exit 1
fi
mkdir -p "$build_dir"
exec 9>"$build_dir/.run.lock"
flock -n 9 || {
    printf 'This project\047s GUI launch script is already running. Close it before relaunching.\n' >&2
    exit 1
}

printf '[1/4] Ensure Qt/OpenCV development tools and device ACL tools\n'
missing=0
for tool in qmake getfacl setfacl; do command -v "$tool" >/dev/null 2>&1 || missing=1; done
if command -v qmake >/dev/null 2>&1; then
    headers=$(qmake -query QT_INSTALL_HEADERS 2>/dev/null || true)
    [ -r "$headers/QtWidgets/QApplication" ] || missing=1
fi
[ -r /usr/include/opencv4/opencv2/imgproc.hpp ] || missing=1
if [ "$missing" -ne 0 ]; then
    as_root apt-get update
    as_root apt-get install -y --no-remove qtbase5-dev qt5-qmake libopencv-core-dev libopencv-imgproc-dev acl
fi

printf '[2/4] Build the current source in build/qt (old binaries are not reused)\n'
sh "$repo_dir/scripts/check-board-env.sh"
make -C "$repo_dir/drivers/gpio-key" -j2
qmake -v
if ! (cd "$build_dir" && qmake "$repo_dir/car/car.pro" && make -j2) > "$build_dir/build.log" 2>&1; then
    printf 'Qt build failed; diagnostics from %s:\n' "$build_dir/build.log" >&2
    tail -n 60 "$build_dir/build.log"
    exit 1
fi
printf 'Qt build complete: %s/car (full build log: %s/build.log)\n' "$build_dir" "$build_dir"
trap cleanup 0
trap 'exit 130' INT
trap 'exit 143' TERM

printf '[3/4] Bind the key and grant access only to this key and selected camera\n'
if [ ! -d /sys/module/car_gpio_key ]; then
    as_root insmod "$module_file" "run_id=$run_id"
    owned_by_this_run || { printf 'Cannot confirm key module ownership.\n' >&2; exit 1; }
fi
if [ "$(readlink -f /sys/bus/platform/devices/car-camera-key/driver 2>/dev/null || true)" != /sys/bus/platform/drivers/car-gpio-key ]; then
    printf 'Key module did not bind to car-camera-key.\n' >&2
    as_root dmesg | tail -n 20
    exit 1
fi
for name in /sys/class/input/event*/device/name; do
    [ -r "$name" ] || continue
    [ "$(cat "$name")" = car-gpio-key ] || continue
    event=${name#/sys/class/input/}
    event=${event%%/*}
    if [ -n "$key_device" ]; then printf 'Multiple car-gpio-key devices.\n' >&2; exit 1; fi
    key_device="/dev/input/$event"
done
[ -n "$key_device" ] || { printf 'No car-gpio-key event device found.\n' >&2; exit 1; }
video_device=$(as_root env "CAR_CAMERA_DEVICE=${CAR_CAMERA_DEVICE:-}" "$build_dir/car" --find-camera)
[ -c "$video_device" ] || { printf 'Camera discovery returned an invalid device.\n' >&2; exit 1; }
acl_dir=$(mktemp -d /tmp/car-gui-acl.XXXXXX)
if [ ! -r "$key_device" ]; then
    key_inode=$(stat -Lc '%d:%i:%t:%T' "$key_device")
    as_root getfacl -p "$key_device" > "$acl_dir/key.acl"
    as_root setfacl -m "u:$(id -u):r" "$key_device"
fi
if [ ! -r "$video_device" ] || [ ! -w "$video_device" ]; then
    video_inode=$(stat -Lc '%d:%i:%t:%T' "$video_device")
    as_root getfacl -p "$video_device" > "$acl_dir/video.acl"
    as_root setfacl -m "u:$(id -u):rw" "$video_device"
fi
printf 'Key: %s; camera: %s\n' "$key_device" "$video_device"

printf '[4/4] Launch GUI on the current VNC desktop\n'
printf 'Press K1 to open the camera. Use the camera Back button to return.\n'
cd "$repo_dir/car"
CAR_CAMERA_DEVICE="$video_device" "$build_dir/car"
