#!/usr/bin/env python3
"""Generate a reviewable in-tree patch; never modify the target kernel tree."""
import argparse
import difflib
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("kernel", type=Path, help="kernel checkout with drivers/misc checked out")
    parser.add_argument("--output", type=Path, default=ROOT / "build/bsp/car-mpu6050.patch")
    args = parser.parse_args()
    kernel = args.kernel.resolve()
    parents = {name: (kernel / name).read_text() for name in ("drivers/misc/Kconfig", "drivers/misc/Makefile")}
    if any("CAR_MPU6050" in text or "car_mpu6050/" in text for text in parents.values()):
        parser.error("integration already present; inspect it instead of appending duplicates")
    if (kernel / "drivers/misc/car_mpu6050").exists():
        parser.error("target directory already exists")
    source = (ROOT / "drivers/mpu6050/car_mpu6050.c").read_text()
    old_include = '#include "../../include/car_mpu6050_uapi.h"'
    if source.count(old_include) != 1:
        parser.error("driver include changed; inspect before generating integration")
    menu, separator, tail = parents["drivers/misc/Kconfig"].rpartition("endmenu")
    if not separator or tail.strip():
        parser.error("unexpected misc Kconfig structure")
    files = {
        "drivers/misc/Kconfig": menu + 'source "drivers/misc/car_mpu6050/Kconfig"\nendmenu' + tail,
        "drivers/misc/Makefile": parents["drivers/misc/Makefile"].rstrip("\n") + '\nobj-$(CONFIG_CAR_MPU6050) += car_mpu6050/\n',
        "drivers/misc/car_mpu6050/Kconfig": (ROOT / "bsp/car-mpu6050.Kconfig").read_text(),
        "drivers/misc/car_mpu6050/Makefile": 'obj-$(CONFIG_CAR_MPU6050) += car_mpu6050.o\n',
        "drivers/misc/car_mpu6050/car_mpu6050.c": source.replace(old_include, '#include "car_mpu6050_uapi.h"'),
        "drivers/misc/car_mpu6050/car_mpu6050_uapi.h": (ROOT / "include/car_mpu6050_uapi.h").read_text(),
    }
    patch = []
    for name, updated in files.items():
        original = parents.get(name, "")
        patch.append(f"diff --git a/{name} b/{name}\n")
        if name not in parents:
            patch.append("new file mode 100644\n")
        patch.extend(difflib.unified_diff(original.splitlines(True), updated.splitlines(True),
                     fromfile="a/" + name if name in parents else "/dev/null", tofile="b/" + name))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("".join(patch))
    subprocess.run(["git", "-C", str(kernel), "apply", "--check", str(args.output.resolve())], check=True)
    print(f"Prepared and git apply --check passed: {args.output}")
    print("Kernel tree unchanged. Review patch before applying; build configuration remains a separate step.")


if __name__ == "__main__":
    main()
