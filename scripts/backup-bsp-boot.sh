#!/bin/sh
# Read the verified boot partition into project files. Never write block devices.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
device=/dev/mmcblk0p2
[ "$(findmnt -n -o SOURCE -T /boot)" = "$device" ] || {
    printf 'STOP: /boot is not the previously verified mmcblk0p2.\n' >&2; exit 1;
}
bytes=$(( $(cat /sys/class/block/mmcblk0p2/size) * 512 ))
start=$(cat /sys/class/block/mmcblk0p2/start)
mkdir -p "$repo/build/bsp"
available=$(df -Pk "$repo/build/bsp" | awk 'NR==2 {print $4}')
[ "$((available * 1024))" -gt "$((bytes * 3))" ] || {
    printf 'STOP: need space for raw and compressed backup files.\n' >&2; exit 1;
}
output=$(mktemp -d "$repo/build/bsp/boot-backup.XXXXXX")
printf 'Keep boot files unchanged during backup. Output: %s\n' "$output"
sync
if [ "$(id -u)" -eq 0 ]; then
    dd if="$device" bs=4M status=progress > "$output/original-boot.img"
else
    sudo dd if="$device" bs=4M status=progress > "$output/original-boot.img"
fi
[ "$(stat -c %s "$output/original-boot.img")" -eq "$bytes" ] || {
    printf 'STOP: backup length does not match partition size.\n' >&2; exit 1;
}
gzip -c "$output/original-boot.img" > "$output/original-boot.img.gz"
gzip -t "$output/original-boot.img.gz"
{
    printf 'Kernel: %s\nDevice: %s\nPartition bytes: %s\nStart sector (512 bytes): %s\n' "$(uname -r)" "$device" "$bytes" "$start"
    printf 'Scope: boot partition snapshot only; not a full eMMC backup or a tested restore.\n'
    (cd "$output" && sha256sum original-boot.img original-boot.img.gz)
} > "$output/manifest.txt"
cat "$output/manifest.txt"
ls -lh "$output/original-boot.img.gz"
printf 'Copy original-boot.img.gz and manifest.txt to your computer. No boot files or block devices were written.\n'
