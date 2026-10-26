"""Read-only runtime device-tree validation, without real board paths."""
import importlib.util
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("bsp_runtime", ROOT / "scripts/check-bsp-runtime.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class RuntimeTreeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.base = Path(self.temp.name)
        strings = {
            "model": "EmbedFire LubanCat-3",
            "car-camera-key/compatible": "saberddx,car-gpio-key",
            "car-camera-key/status": "okay",
            "i2c@2aca0000/status": "okay",
            "i2c@2aca0000/mpu6050@68/compatible": "saberddx,car-mpu6050",
            "i2c@2aca0000/mpu6050@68/status": "okay",
        }
        for path, value in strings.items():
            self.write(path, value.encode() + b"\0")
        props = {
            "i2c@2aca0000/clock-frequency": (100000,),
            "i2c@2aca0000/mpu6050@68/reg": (0x68,),
            "car-camera-key/linux,code": (212,),
            "car-camera-key/debounce-interval": (20,),
            "pinctrl/gpio@2ae40000/phandle": (9,),
            "car-camera-key/button-gpios": (9, 6, 1),
            "pinctrl/pcfg-pull-up/phandle": (12,),
            "pinctrl/car-keys/car-key-pin/rockchip,pins": (4, 6, 0, 12),
            "pinctrl/car-keys/car-key-pin/phandle": (101,),
            "car-camera-key/pinctrl-0": (101,),
            "pinctrl/i2c7/i2c7m1-xfer/phandle": (202,),
            "i2c@2aca0000/pinctrl-0": (202,),
        }
        for path, values in props.items():
            self.write_cells(path, values)

    def tearDown(self):
        self.temp.cleanup()

    def write(self, name, data):
        path = self.base / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)

    def write_cells(self, path, values):
        self.write(path, struct.pack(">" + "I" * len(values), *values))

    def test_correct_tree_and_big_endian_cells(self):
        report = MODULE.audit_tree(self.base)
        self.assertEqual(report["model"], "EmbedFire LubanCat-3")
        self.assertIn("0x68", report["mpu6050"])

    def test_wrong_sensor_address_rejected(self):
        self.write_cells("i2c@2aca0000/mpu6050@68/reg", (0x69,))
        with self.assertRaisesRegex(ValueError, "sensor address"):
            MODULE.audit_tree(self.base)

    def test_wrong_polarity_and_pullup_rejected(self):
        self.write_cells("car-camera-key/button-gpios", (9, 6, 0))
        with self.assertRaisesRegex(ValueError, "active low"):
            MODULE.audit_tree(self.base)
        self.write_cells("car-camera-key/button-gpios", (9, 6, 1))
        self.write_cells("pinctrl/car-keys/car-key-pin/rockchip,pins", (4, 6, 0, 13))
        with self.assertRaisesRegex(ValueError, "pull-up"):
            MODULE.audit_tree(self.base)

    def test_wrong_i2c_pin_group_rejected(self):
        self.write_cells("i2c@2aca0000/pinctrl-0", (203,))
        with self.assertRaisesRegex(ValueError, "I2C7 M1"):
            MODULE.audit_tree(self.base)

    def test_truncated_cells_rejected(self):
        self.write("car-camera-key/linux,code", b"\0")
        with self.assertRaisesRegex(ValueError, "Invalid device-tree cells"):
            MODULE.audit_tree(self.base)

    def test_cloud_entry_points_refuse_before_board_operations(self):
        for command in (["python3", str(ROOT / "scripts/check-bsp-runtime.py")],
                        ["sh", str(ROOT / "scripts/test-bsp-runtime.sh")]):
            result = subprocess.run(command, capture_output=True, text=True, timeout=10)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("STOP:", result.stderr)


if __name__ == "__main__":
    unittest.main()
