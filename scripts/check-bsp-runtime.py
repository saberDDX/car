#!/usr/bin/env python3
"""Read actual experimental-kernel configuration, device tree and bindings."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import platform
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
RELEASE = "6.1.99-rk3576-car-bsp"


def text(path):
    return path.read_bytes().rstrip(b"\0\n").decode()


def cells(path):
    data = path.read_bytes()
    if not data or len(data) % 4:
        raise ValueError(f"Invalid device-tree cells: {path}")
    return struct.unpack(">" + "I" * (len(data) // 4), data)


def require(actual, expected, label):
    if actual != expected:
        raise ValueError(f"{label}: expected {expected!r}, found {actual!r}")


def audit_tree(base):
    key = base / "car-camera-key"
    i2c = base / "i2c@2aca0000"
    sensor = i2c / "mpu6050@68"
    require(text(base / "model"), "EmbedFire LubanCat-3", "model")
    for node, compatible in ((key, "saberddx,car-gpio-key"), (sensor, "saberddx,car-mpu6050")):
        require(text(node / "compatible"), compatible, "compatible")
        require(text(node / "status"), "okay", "device status")
    require(text(i2c / "status"), "okay", "I2C7 status")
    require(cells(i2c / "clock-frequency"), (100000,), "I2C clock")
    require(cells(sensor / "reg"), (0x68,), "sensor address")
    require(cells(key / "linux,code"), (212,), "KEY_CAMERA")
    require(cells(key / "debounce-interval"), (20,), "debounce")
    gpio4 = cells(base / "pinctrl/gpio@2ae40000/phandle")[0]
    require(cells(key / "button-gpios"), (gpio4, 6, 1), "GPIO4_A6 active low")
    pullup = cells(base / "pinctrl/pcfg-pull-up/phandle")[0]
    pin = base / "pinctrl/car-keys/car-key-pin"
    require(cells(pin / "rockchip,pins"), (4, 6, 0, pullup), "GPIO mux and pull-up")
    require(cells(key / "pinctrl-0"), cells(pin / "phandle"), "key pinctrl reference")
    xfer = base / "pinctrl/i2c7/i2c7m1-xfer"
    require(cells(i2c / "pinctrl-0"), cells(xfer / "phandle"), "I2C7 M1 reference")
    return {"model": "EmbedFire LubanCat-3", "key": "GPIO4_A6 active low, pull-up, KEY_CAMERA, 20ms",
            "mpu6050": "I2C7 M1, 100kHz, address 0x68"}


def audit_modules():
    result = {}
    for name in ("car_gpio_key", "car_mpu6050"):
        def info(field):
            return subprocess.check_output(["modinfo", "-k", RELEASE, "-F", field, name], text=True).strip()
        path = Path(info("filename")).resolve(strict=True)
        path.relative_to(Path("/lib/modules", RELEASE).resolve(strict=True))
        vermagic = info("vermagic")
        if not vermagic.startswith(RELEASE + " "):
            raise ValueError(f"Wrong vermagic for {name}")
        result[name] = {"file": str(path), "vermagic": vermagic}
        if name == "car_mpu6050":
            require(info("intree"), "Y", "MPU6050 Kbuild intree marker")
            result[name]["intree"] = "Y"
    return result


def audit_bindings():
    node = Path("/sys/firmware/devicetree/base/i2c@2aca0000/mpu6050@68")
    clients = [path for path in Path("/sys/bus/i2c/devices").glob("*-0068")
               if (path / "of_node").resolve() == node]
    if len(clients) != 1:
        raise ValueError("MPU6050 client is missing or ambiguous")
    client = clients[0]
    require((client / "driver").resolve(), Path("/sys/bus/i2c/drivers/car-mpu6050"), "MPU binding")
    require(text(client / "who_am_i"), "0x68", "live sensor identity")
    require(Path("/sys/class/misc/car_mpu6050/device").resolve(), client.resolve(), "misc owner")
    if not Path("/dev/car_mpu6050").is_char_device():
        raise ValueError("MPU6050 misc node absent")
    key = Path("/sys/bus/platform/devices/car-camera-key")
    require((key / "of_node").resolve(), Path("/sys/firmware/devicetree/base/car-camera-key"), "key OF node")
    require((key / "driver").resolve(), Path("/sys/bus/platform/drivers/car-gpio-key"), "key binding")
    events = [path.name for path in Path("/sys/class/input").glob("event*")
              if (path / "device/name").exists() and text(path / "device/name") == "car-gpio-key"
              and (path / "device").resolve().is_relative_to(key.resolve())]
    if len(events) != 1:
        raise ValueError("Expected exactly one input event device from the key platform device")
    return {"mpu_client": client.name, "who_am_i": "0x68", "key_event": "/dev/input/" + events[0]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if platform.machine() != "aarch64" or platform.release() != RELEASE:
        raise ValueError("Run only on the board booted into " + RELEASE)
    record = json.loads((ROOT / "bsp/arm64-build-result.json").read_text())
    with gzip.open("/proc/config.gz", "rb") as stream:
        config = stream.read()
    config_sha = hashlib.sha256(config).hexdigest()
    require(config_sha, record["files"][".config"]["sha256"], "running build configuration SHA256")
    require(b"CONFIG_CAR_MPU6050=m\n" in config, True, "integrated driver configuration")
    cameras = [path.name for path in Path("/sys/class/video4linux").glob("video*")
               if (path / "device/driver").resolve().name == "uvcvideo"]
    report = {"release": RELEASE, "running_config_sha256": config_sha,
              "tree": audit_tree(Path("/sys/firmware/devicetree/base")),
              "modules": audit_modules(), "bindings": audit_bindings(),
              "usb_video_nodes": sorted(cameras),
              "scope": "Configuration/tree/module binding checks; sensor sampling, key events and video streaming need separate checks."}
    print(json.dumps(report, indent=2))
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + "\n")
    print("PASS: actual build configuration, live device tree, matching modules and project device bindings.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        raise SystemExit(f"STOP: {error}")
