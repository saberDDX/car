"""Legacy boot image validation without executing boot commands."""
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
import zlib

SPEC = importlib.util.spec_from_file_location("bsp_inputs", Path(__file__).resolve().parents[1] / "scripts/analyze-bsp-inputs.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class BoardInputsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        boot = self.root / "boot"
        (boot / "uEnv").mkdir(parents=True)
        (self.root / "running.config").write_text("CONFIG_I2C=y\n")
        (boot / "config-6.1.99-rk3576").write_text("CONFIG_I2C=y\n")
        command = b"echo test;\n"
        (boot / "boot.cmd").write_bytes(command)
        payload = struct.pack(">II", len(command), 0) + command
        header = bytearray(struct.pack(">7I4B32s", 0x27051956, 0, 0, len(payload), 0, 0,
                                      zlib.crc32(payload), 5, 2, 6, 0, b"test"))
        header[4:8] = struct.pack(">I", zlib.crc32(header))
        (boot / "boot.scr").write_bytes(header + payload)
        (boot / "uEnv/uEnvLubanCat3.txt").write_text(
            'uname_r=6.1.99-rk3576\n#dtoverlay=ignored\ndtoverlay=/key.dtbo\ndtoverlay=/mpu.dtbo\n')

    def tearDown(self):
        self.temp.cleanup()

    def test_valid_image_and_multiple_active_overlays(self):
        result = MODULE.analyze(self.root)
        self.assertTrue(result["boot_scr_matches_boot_cmd"])
        self.assertEqual(result["overlays"], ["/key.dtbo", "/mpu.dtbo"])

    def test_rejects_mismatched_running_config(self):
        (self.root / "running.config").write_text("different\n")
        with self.assertRaisesRegex(ValueError, "configurations differ"):
            MODULE.analyze(self.root)

    def test_rejects_corrupt_payload_and_header(self):
        path = self.root / "boot/boot.scr"
        original = path.read_bytes()
        for offset in (10, len(original) - 1):
            data = bytearray(original)
            data[offset] ^= 1
            path.write_bytes(data)
            with self.assertRaisesRegex(ValueError, "CRC"):
                MODULE.analyze(self.root)

    def test_rejects_stale_script_image(self):
        (self.root / "boot/boot.cmd").write_text("echo another;\n")
        with self.assertRaisesRegex(ValueError, "supplied boot.cmd"):
            MODULE.analyze(self.root)
