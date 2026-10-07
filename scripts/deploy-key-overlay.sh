#!/bin/sh
# Validate again immediately before the root helper changes the boot files.
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
command -v python3 >/dev/null 2>&1 || {
    printf 'STOP: python3 is required for configuration backup and deployment.\n' >&2
    exit 1
}
sh "$repo_dir/scripts/prepare-key-overlay.sh"
overlay="$repo_dir/drivers/gpio-key/build/car-lubancat3-gpio4-a6-key.dtbo"
if [ "$(id -u)" -eq 0 ]; then
    python3 "$repo_dir/scripts/install-key-overlay.py" "$overlay"
else
    sudo python3 "$repo_dir/scripts/install-key-overlay.py" "$overlay"
fi
