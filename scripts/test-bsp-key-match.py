#!/usr/bin/env python3
"""Reproduce a platform match failure and restore the original key binding."""
import argparse
import datetime
import fcntl
import json
import os
from pathlib import Path
import platform
import signal
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
RELEASE = "6.1.99-rk3576-car-bsp"
COMPATIBLE = "saberddx,car-gpio-key"
DRIVER = Path("/sys/bus/platform/drivers/car-gpio-key")


class KeyDevice:
    def __init__(self):
        self.device = Path("/sys/bus/platform/devices/car-camera-key")
        self.override_path = self.device / "driver_override"

    def override(self):
        value = self.override_path.read_text().strip()
        return "" if value == "(null)" else value

    def set_override(self, value):
        self.override_path.write_text(value + "\n")

    def unbind(self):
        (DRIVER / "unbind").write_text(self.device.name + "\n")

    def probe(self):
        Path("/sys/bus/platform/drivers_probe").write_text(self.device.name + "\n")

    def snapshot(self):
        events = []
        for path in Path("/sys/class/input").glob("event*"):
            node = path / "device/name"
            if node.exists() and node.read_text().strip() == "car-gpio-key" and (path / "device").resolve().is_relative_to(self.device.resolve()):
                events.append("/dev/input/" + path.name)
        return {"device_present": self.device.exists(),
                "compatible": (self.device / "of_node/compatible").read_bytes().rstrip(b"\0").decode(),
                "module_loaded": Path("/sys/module/car_gpio_key").is_dir(),
                "driver": str((self.device / "driver").resolve()) if (self.device / "driver").is_symlink() else None,
                "driver_override": self.override(), "events": sorted(events)}

    def wait_ready(self):
        for _ in range(40):
            state = self.snapshot()
            if state["driver"] == str(DRIVER) and len(state["events"]) == 1:
                return state
            time.sleep(0.05)
        raise RuntimeError("Key binding/input device did not recover within 2 seconds")


def exercise(device, marker, emit=print):
    before = device.snapshot()
    if (not before["device_present"] or before["compatible"] != COMPATIBLE
            or not before["module_loaded"] or before["driver"] != str(DRIVER)
            or len(before["events"]) != 1 or before["driver_override"]):
        raise ValueError("Need the expected loaded/bound key with one input device and an empty driver_override")
    emit("BEFORE: " + json.dumps(before))
    changed = False
    try:
        # Set before unbinding so a concurrent rescan cannot rebind the key.
        changed = True
        device.set_override(marker)
        device.unbind()
        device.probe()
        fault = device.snapshot()
        if (not fault["device_present"] or not fault["module_loaded"]
                or fault["compatible"] != COMPATIBLE or fault["driver"] is not None
                or fault["events"] or fault["driver_override"] != marker):
            raise RuntimeError("Did not reproduce the expected match-blocked state")
        emit("FAULT: " + json.dumps(fault))
        emit("Matched symptom: platform device + compatible + module exist; driver link and input device are absent.")
    finally:
        if changed:
            current = device.override()
            if current not in (marker, ""):
                raise RuntimeError("Another owner changed driver_override; will not overwrite their value")
            device.set_override("")
            state = device.snapshot()
            if state["driver"] is None:
                device.probe()
            restored = device.wait_ready()
            if restored["driver_override"]:
                raise RuntimeError("driver_override was not cleared")
            emit("RESTORED: " + json.dumps(restored))
    return {"before": before, "fault": fault, "restored": restored}


def interrupted(signum, _frame):
    raise SystemExit(128 + signum)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    if os.geteuid() != 0 or platform.machine() != "aarch64" or platform.release() != RELEASE:
        raise ValueError("Run with sudo only on the verified experimental LubanCat-3 kernel")
    if Path("/sys/firmware/devicetree/base/model").read_bytes().rstrip(b"\0\n") != b"EmbedFire LubanCat-3":
        raise ValueError("Unexpected board model")
    monitor = ROOT / "tools/key-monitor/key-monitor"
    if not monitor.is_file() or not os.access(monitor, os.X_OK):
        raise ValueError("Run test-bsp-runtime.sh first to build the native event monitor")
    for sig in (signal.SIGTERM, signal.SIGHUP):
        signal.signal(sig, interrupted)
    token = "car-bsp-match-" + uuid.uuid4().hex
    marker = "car-intentionally-unmatched-" + uuid.uuid4().hex[:12]
    with Path("/run/lock/car-bsp-key-match.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        try:
            if not Path("/sys/module/car_gpio_key").is_dir():
                subprocess.run(["modprobe", "car_gpio_key", "run_id=" + token], check=True)
            vermagic = subprocess.check_output(["modinfo", "-F", "vermagic", "car_gpio_key"], text=True)
            if not vermagic.startswith(RELEASE + " "):
                raise ValueError("Installed key module does not match the experiment")
            device = KeyDevice()
            device.wait_ready()
            record = exercise(device, marker, emit=lambda line: print(line, flush=True))
            print("Press/release K1 ONCE after Verify, holding it for 3 seconds.", flush=True)
            subprocess.run(["timeout", "--foreground", "60", str(monitor), "--verify", "1",
                            record["restored"]["events"][0]], check=True)
            record.update({"release": RELEASE, "fault_kind": "platform driver_override blocks matching",
                           "physical_key_after_restore": "PASS", "boot_or_device_tree_changed": False,
                           "scope": "Deliberate match failure, automatic rebind and physical input verification; not a partition restore test."})
            folder = ROOT / "build/bsp"
            folder.mkdir(parents=True, exist_ok=True)
            stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
            report = folder / ("key-match-" + stamp + "-" + str(os.getpid()) + ".json")
            report.write_text(json.dumps(record, indent=2) + "\n")
            print("PASS: controlled matching failure, cleared override, recovered binding/input and physical key event.")
            print("Report: " + str(report))
        finally:
            parameter = Path("/sys/module/car_gpio_key/parameters/run_id")
            if parameter.exists() and parameter.read_text().strip() == token:
                subprocess.run(["rmmod", "car_gpio_key"], check=True)
                print("Cleaned up only the module loaded by this run; initially loaded modules are retained.")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        raise SystemExit("Interrupted; restoration was attempted before exiting.")
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"STOP: {error}", file=sys.stderr)
        sys.exit(1)
