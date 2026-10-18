"""Real DT compilation/merge and boot-file preservation tests, without /boot."""
import contextlib
import importlib.util
import io
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("mpu_install", ROOT / "scripts/install-mpu6050-overlay.py")
mpu_install = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(mpu_install)
installer = mpu_install.installer
KERNEL = "6.1.99-rk3576"
KEY_LINE = "dtoverlay=/dtb/overlay/car-lubancat3-gpio4-a6-key.dtbo"
MPU_LINE = f"dtoverlay=/dtb/overlay/{mpu_install.OVERLAY_NAME}"
CONFIG = (f"uname_r={KERNEL}\nenable_uboot_overlays=1\n#overlay_start\n"
          f"{KEY_LINE}\ndtoverlay=/dtb/overlay/screen.dtbo\n#overlay_end\n")
BASE = """/dts-v1/;
/ {
    model = "EmbedFire LubanCat-3";
    compatible = "embedfire,rk3576-lubancat-3", "rockchip,rk3576";
    #address-cells = <2>;
    #size-cells = <2>;
    i2c7: i2c@2aca0000 {
        compatible = "rockchip,rk3576-i2c";
        reg = <0 0x2aca0000 0 0x1000>;
        #address-cells = <1>;
        #size-cells = <0>;
        status = "disabled";
    };
    pinctrl {
        pcfg_pull_none_smt: pcfg-pull-none-smt {
            bias-disable;
            input-schmitt-enable;
        };
        i2c7m1_xfer: i2c7m1-xfer {
            rockchip,pins = <3 0 11 &pcfg_pull_none_smt>,
                            <3 1 11 &pcfg_pull_none_smt>;
        };
    };
    car-camera-key {
        compatible = "saberddx,car-gpio-key";
        linux,code = <212>;
        debounce-interval = <20>;
    };
};
"""


class MpuDeploymentTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="car-mpu-install-")
        self.root = Path(self.temporary.name)
        self.config = self.root / "uEnvLubanCat3.txt"
        self.config.write_text(CONFIG)
        self.config.chmod(0o640)
        self.selector = self.root / "uEnv.txt"
        self.selector.symlink_to(self.config.name)
        self.overlays = self.root / "overlay"
        self.overlays.mkdir()
        self.key_blob = self.overlays / "car-lubancat3-gpio4-a6-key.dtbo"
        self.key_blob.write_bytes(b"existing key blob")
        self.source = self.root / "mpu.dtbo"
        self.source.write_bytes(b"\xd0\x0d\xfe\xed" + b"\0" * 40)
        self.destination = self.overlays / mpu_install.OVERLAY_NAME
        self.sync_patch = patch.object(installer.os, "sync")
        self.sync_patch.start()

    def tearDown(self):
        self.sync_patch.stop()
        self.temporary.cleanup()

    def deploy(self):
        with contextlib.redirect_stdout(io.StringIO()):
            installer.deploy(self.selector.resolve(), self.overlays, self.source, KERNEL,
                             overlay_name=mpu_install.OVERLAY_NAME, backup_tag="car-mpu6050")

    def test_keeps_key_screen_symlink_permissions_and_is_idempotent(self):
        self.deploy()
        self.assertEqual(self.config.read_text().replace(MPU_LINE + "\n", ""), CONFIG)
        self.assertEqual(self.config.read_text().splitlines().count(KEY_LINE), 1)
        self.assertEqual(self.key_blob.read_bytes(), b"existing key blob")
        self.assertTrue(self.selector.is_symlink())
        self.assertEqual(self.config.stat().st_mode & 0o777, 0o640)
        self.assertEqual(self.destination.read_bytes(), self.source.read_bytes())
        backups = list(self.root.glob("*.car-mpu6050-backup-*"))
        self.assertEqual(len(backups), 1)
        self.assertEqual(backups[0].read_text(), CONFIG)
        self.deploy()
        self.assertEqual(len(list(self.root.glob("*.car-mpu6050-backup-*"))), 1)

    def test_failure_after_config_replacement_restores_key_and_both_files(self):
        self.destination.write_bytes(b"old MPU blob")
        original_write = installer.atomic_write
        injected = False

        def fail_after_replace(path, contents, file_stat=None):
            nonlocal injected
            original_write(path, contents, file_stat)
            if path == self.config and not injected:
                injected = True
                raise OSError("simulated directory fsync failure")

        with patch.object(installer, "atomic_write", fail_after_replace):
            with self.assertRaises(OSError):
                self.deploy()
        self.assertEqual(self.config.read_text(), CONFIG)
        self.assertEqual(self.destination.read_bytes(), b"old MPU blob")
        self.assertEqual(self.key_blob.read_bytes(), b"existing key blob")

    def test_rejects_duplicate_mpu_entry_and_disabled_uboot(self):
        for text in (CONFIG.replace("#overlay_end", (MPU_LINE + "\n") * 2 + "#overlay_end"),
                     CONFIG.replace("enable_uboot_overlays=1", "enable_uboot_overlays=0")):
            with self.subTest(text=text):
                self.config.write_text(text)
                with self.assertRaises(ValueError):
                    self.deploy()
                self.assertEqual(self.config.read_text(), text)
                self.assertFalse(self.destination.exists())


@unittest.skipUnless(all(shutil.which(tool) for tool in ("dtc", "fdtoverlay", "fdtget")),
                     "device-tree-compiler tools are required")
class MpuOverlayTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="car-mpu-dt-")
        self.root = Path(self.temporary.name)

    def tearDown(self):
        self.temporary.cleanup()

    def build(self, source=BASE):
        dts = self.root / "base.dts"
        dtb = self.root / "base.dtb"
        dts.write_text(source)
        subprocess.run(["dtc", "-@", "-I", "dts", "-O", "dtb", "-o", str(dtb), str(dts)],
                       check=True, capture_output=True, text=True)
        return subprocess.run(["sh", str(ROOT / "scripts/build-mpu6050-overlay.sh"),
                               str(dtb), str(self.root / "output")], capture_output=True, text=True)

    def test_merge_keeps_key_and_configures_mpu(self):
        result = self.build()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        merged = self.root / "output/merged-mpu6050.dtb"
        code = subprocess.check_output(["fdtget", "-t", "u", str(merged),
                                        "/car-camera-key", "linux,code"], text=True).strip()
        self.assertEqual(code, "212")
        # A second deployment cannot silently take over an already-enabled controller.
        second = subprocess.run(["sh", str(ROOT / "scripts/build-mpu6050-overlay.sh"),
                                  str(merged), str(self.root / "second")], capture_output=True, text=True)
        self.assertNotEqual(second.returncode, 0)
        self.assertIn("unused I2C7", second.stderr)

    def test_rejects_wrong_board_enabled_controller_wrong_pins_and_pull(self):
        variants = [
            BASE.replace("EmbedFire LubanCat-3", "Other board"),
            BASE.replace('status = "disabled"', 'status = "okay"'),
            BASE.replace("<3 1 11", "<3 2 11"),
            BASE.replace("input-schmitt-enable;", ""),
            BASE.replace("i2c7:", "other_i2c:"),
            BASE.replace("pcfg_pull_none_smt:", "other_pull:").replace("&pcfg_pull_none_smt", "&other_pull"),
        ]
        for source in variants:
            with self.subTest(source=source):
                result = self.build(source)
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_rejects_existing_address_even_with_different_node_name(self):
        source = BASE.replace('status = "disabled";',
                              'status = "disabled"; another@68 { reg = <0x68>; };')
        result = self.build(source)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("already has a device at 0x68", result.stderr)


if __name__ == "__main__":
    unittest.main()
