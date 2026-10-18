#!/bin/sh
# Board-only, reuses the identity test's ownership/binding/cleanup checks.
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
exec sh "$repo_dir/scripts/test-mpu6050.sh" --data "$@"
