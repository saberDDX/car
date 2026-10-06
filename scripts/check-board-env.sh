#!/bin/sh
# Read-only checks; do not print environment variables or credentials.
set -eu

kernel_release=$(uname -r)
header_tree="/lib/modules/$kernel_release/build"

printf 'Architecture: %s\n' "$(uname -m)"
printf 'Running kernel: %s\n' "$kernel_release"
printf 'Header path: %s\n' "$header_tree"

missing=0
for tool in make gcc insmod rmmod; do
    if command -v "$tool" >/dev/null 2>&1; then
        printf '%s: found\n' "$tool"
    else
        printf '%s: missing\n' "$tool"
        missing=1
    fi
done

for relative_path in Makefile include/generated/autoconf.h include/config/auto.conf include/generated/asm-offsets.h include/generated/bounds.h; do
    if [ -f "$header_tree/$relative_path" ]; then
        printf 'Header file %s: found\n' "$relative_path"
    else
        printf 'Header file %s: missing\n' "$relative_path"
        missing=1
    fi
done

if [ -f "$header_tree/scripts/mod/modpost" ] && [ -x "$header_tree/scripts/mod/modpost" ]; then
    printf 'Kbuild modpost: found\n'
else
    printf 'Kbuild modpost: missing or not executable\n'
    missing=1
fi

if [ -f "$header_tree/include/config/auto.conf" ]; then
    compiler_text=$(awk '/^CONFIG_CC_VERSION_TEXT=/ { sub(/^[^=]*=/, ""); print; exit }' "$header_tree/include/config/auto.conf")
    case "$compiler_text" in
        \"*)
            printf 'CONFIG_CC_VERSION_TEXT: unexpected surrounding quotes in auto.conf; check vendor Kbuild before compiling.\n'
            missing=1
            ;;
    esac
fi

for option in CONFIG_MODULES CONFIG_MODVERSIONS CONFIG_MODULE_SIG_FORCE; do
    if [ -f "$header_tree/include/config/auto.conf" ]; then
        setting=$(awk -F= -v name="$option" '$1 == name { print $2 }' "$header_tree/include/config/auto.conf")
        printf '%s: %s\n' "$option" "${setting:-not enabled}"
        if [ "$option" = CONFIG_MODULES ] && [ "$setting" != y ]; then
            printf 'This kernel configuration does not enable loadable modules.\n'
            missing=1
        fi
        if [ "$option" = CONFIG_MODVERSIONS ] && [ "$setting" = y ] && [ ! -f "$header_tree/Module.symvers" ]; then
            printf 'Module.symvers: missing (required for this versioned-symbol build)\n'
            missing=1
        fi
    fi
done

if command -v modinfo >/dev/null 2>&1; then
    printf 'modinfo: found\n'
else
    printf 'modinfo: missing\n'
    missing=1
fi

if [ "$missing" -ne 0 ]; then
    printf 'Prerequisite check failed. Report this output before installing or changing anything.\n'
    exit 1
fi

printf 'Basic prerequisites found; actual module compilation and loading still need verification.\n'
