#!/bin/sh
# Download signed-APT packages and extract into a local SDK; no system install.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
sdk=${BSP_SDK_DIR:-/workspace/bsp-sdk}
mkdir -p "$sdk/packages" "$sdk/root" "$sdk/lists/partial" "$sdk/empty" "$sdk/archives/partial"
sdk=$(realpath "$sdk")
manifest=$(sha256sum "$repo/bsp/cloud-packages.txt" | cut -d ' ' -f 1)
if [ -r "$sdk/.prepared-manifest" ] && [ "$(cat "$sdk/.prepared-manifest")" = "$manifest" ]; then
    BSP_SDK_ROOT="$sdk/root"
    export BSP_SDK_ROOT
    . "$repo/bsp/cloud-env.sh"
    printf 'int x;\n' | "$BSP_CC" -x c -c - -o "$sdk/arm64-probe.o"
    file "$sdk/arm64-probe.o"
    printf 'Matching SDK already prepared; no packages were re-extracted.\n'
    exit 0
fi
python3 - "$sdk" <<'PY'
from pathlib import Path
import json
import sys
p = Path(sys.argv[1])
(p / 'sources.list').write_text('deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] https://deb.debian.org/debian bookworm main\n')
entries = {'Dir::Etc::main': '/dev/null', 'Dir::Etc::parts': str(p/'empty'),
           'Dir::Etc::sourcelist': str(p/'sources.list'), 'Dir::Etc::sourceparts': str(p/'empty'),
           'Dir::State::lists': str(p/'lists'), 'Dir::Cache::archives': str(p/'archives'),
           'APT::Sandbox::User': '', 'Acquire::Languages': 'none'}
(p/'apt.conf').write_text(''.join(k+' '+json.dumps(v)+';\n' for k,v in entries.items()))
PY
export APT_CONFIG="$sdk/apt.conf"
apt-get update
(
    cd "$sdk/packages"
    # Each line is one exact package=version from the validated package set.
    while IFS= read -r package; do
        [ -z "$package" ] || apt-get download "$package"
    done < "$repo/bsp/cloud-packages.txt"
)
for package in "$sdk/packages"/*.deb; do dpkg-deb -x "$package" "$sdk/root"; done
if [ -x "$sdk/root/bin/kmod" ]; then
    ln -sfn ../../bin/kmod "$sdk/root/usr/bin/depmod"
fi
BSP_SDK_ROOT="$sdk/root"
export BSP_SDK_ROOT
. "$repo/bsp/cloud-env.sh"
printf 'int x;\n' | "$BSP_CC" -x c -c - -o "$sdk/arm64-probe.o"
file "$sdk/arm64-probe.o"
printf '%s\n' "$manifest" > "$sdk/.prepared-manifest"
printf 'SDK ready. Source bsp/cloud-env.sh before building.\n'
