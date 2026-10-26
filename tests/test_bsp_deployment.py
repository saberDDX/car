"""Verify staging/switch/rollback against actual artifacts in a temporary tree."""
import importlib.util
from pathlib import Path
import shutil
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("bsp_deploy", ROOT / "scripts/deploy-bsp-kernel.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)
PACKAGE = ROOT / "build/bsp/car-bsp-artifacts-with-initrd.tar.gz"
BASE = ROOT / "build/bsp/board-inputs/boot"
INITRD = ROOT / "build/bsp/original-initrd-6.1.img"


class DeploymentPureTests(unittest.TestCase):
    def test_only_two_uenv_lines_change(self):
        original = (BASE / "uEnv/uEnvLubanCat3.txt").read_bytes() if BASE.exists() else None
        if original is None:
            self.skipTest("Board input fixture not present")
        updated = MODULE.experiment_uenv(original)
        changes = [(a, b) for a, b in zip(original.splitlines(), updated.splitlines()) if a != b]
        self.assertEqual(len(changes), 2)
        self.assertEqual(changes[0][1], b"uname_r=6.1.99-rk3576-car-bsp")
        self.assertEqual(changes[1][1], b"initrd=initrd-6.1.99-rk3576-car-bsp")

    def test_rejects_archive_traversal_and_noncanonical_names(self):
        for name in ("../boot/boot.scr", "/boot/Image", "boot/../Image", "boot//Image", ""):
            with self.assertRaises(ValueError):
                MODULE.safe_name(name)

    def test_non_target_stage_refused_before_install(self):
        result = subprocess.run(["python3", str(ROOT / "scripts/deploy-bsp-kernel.py"), "stage", "/nonexistent/package"],
                                capture_output=True, text=True, timeout=10)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("verified LubanCat-3", result.stderr)


@unittest.skipUnless(PACKAGE.exists() and INITRD.exists() and BASE.exists(), "Actual BSP artifacts are not present")
class DeploymentArtifactTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.files = MODULE.load_package(PACKAGE)

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(dir=ROOT / "build/bsp")
        self.root = Path(self.temp.name)
        for name in ("boot.cmd", "boot.scr", "dtb/rk3576-lubancat-3.dtb", "uEnv/uEnvLubanCat3.txt"):
            destination = self.root / "boot" / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(BASE / name, destination)
        shutil.copyfile(INITRD, self.root / "boot/initrd-6.1")
        (self.root / "boot/Image-6.1.99-rk3576").write_bytes(b"original kernel fixture")
        (self.root / "boot/uEnv/uEnv.txt").symlink_to("uEnvLubanCat3.txt")
        (self.root / "boot/rk-kernel.dtb").symlink_to("dtb/rk3576-lubancat-3.dtb")
        (self.root / "lib/modules").mkdir(parents=True)
        self.sync = patch.object(MODULE.os, "sync")
        self.sync.start()

    def tearDown(self):
        self.sync.stop()
        self.temp.cleanup()

    def test_actual_stage_idempotence_failure_recovery_activate_and_rollback(self):
        config = self.root / MODULE.UENV
        original = config.read_bytes()
        baseline = {name: (self.root / name).read_bytes() for name in [*MODULE.PINNED, f"boot/Image-{MODULE.OLD}"]}
        MODULE.stage(self.root, self.files)
        inode = (self.root / f"boot/Image-{MODULE.NEW}").stat().st_ino
        MODULE.stage(self.root, self.files)
        self.assertEqual(inode, (self.root / f"boot/Image-{MODULE.NEW}").stat().st_ino)
        self.assertEqual(config.read_bytes(), original)
        # A corrupt module must prevent the boot selector from changing.
        key = f"lib/modules/{MODULE.NEW}/extra/car_gpio_key.ko"
        (self.root / key).write_bytes(b"broken")
        with self.assertRaisesRegex(ValueError, "changed or is incomplete"):
            MODULE.switch(self.root)
        self.assertEqual(config.read_bytes(), original)
        (self.root / key).write_bytes(self.files[key])
        # Emulate an error after uEnv replacement and ensure the old bytes return.
        writer = MODULE.atomic_write
        failed = False

        def fail_once(path, data, metadata=None):
            nonlocal failed
            writer(path, data, metadata)
            if path == config and not failed:
                failed = True
                raise OSError("simulated failure after replacement")

        with patch.object(MODULE, "atomic_write", side_effect=fail_once):
            with self.assertRaises(OSError):
                MODULE.switch(self.root)
        self.assertEqual(config.read_bytes(), original)
        state_writer = MODULE.save_state
        failed = False

        def fail_state_once(root, state):
            nonlocal failed
            state_writer(root, state)
            if not failed:
                failed = True
                raise OSError("simulated state fsync failure")

        with patch.object(MODULE, "save_state", side_effect=fail_state_once):
            with self.assertRaises(OSError):
                MODULE.switch(self.root)
        self.assertEqual(config.read_bytes(), original)
        self.assertEqual(MODULE.load_state(self.root)[0]["phase"], "staged")
        MODULE.switch(self.root)
        self.assertEqual(config.read_bytes(), MODULE.experiment_uenv(original))
        self.assertTrue((self.root / "boot/uEnv/uEnv.txt").is_symlink())
        MODULE.switch(self.root, rollback=True)
        self.assertEqual(config.read_bytes(), original)
        for name, contents in baseline.items():
            self.assertEqual((self.root / name).read_bytes(), contents)

    def test_rejects_unknown_config_without_creating_staging_state(self):
        config = self.root / MODULE.UENV
        config.write_bytes(config.read_bytes() + b"# changed by owner\n")
        with self.assertRaisesRegex(ValueError, "differs from audited"):
            MODULE.stage(self.root, self.files)
        self.assertFalse((self.root / MODULE.STATE).exists())

    def test_low_space_does_not_delete_original_files_or_install(self):
        with patch.object(MODULE.os, "statvfs", return_value=SimpleNamespace(f_bavail=1, f_frsize=4096)):
            with self.assertRaisesRegex(ValueError, "Insufficient /boot"):
                MODULE.stage(self.root, self.files)
        self.assertFalse((self.root / MODULE.STATE).exists())
        self.assertFalse((self.root / f"boot/Image-{MODULE.NEW}").exists())
        self.assertTrue((self.root / f"boot/Image-{MODULE.OLD}").exists())

    def test_wrong_archive_digest_rejected(self):
        bad = self.root / "bad.tar.gz"
        bad.write_bytes(b"not the pinned archive")
        with self.assertRaisesRegex(ValueError, "Package SHA256 mismatch"):
            MODULE.load_package(bad)


if __name__ == "__main__":
    unittest.main()
