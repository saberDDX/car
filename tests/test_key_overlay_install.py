"""Filesystem and failure-path tests; never touch the machine's /boot."""
import contextlib
import importlib.util
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

HELPER = Path(__file__).resolve().parents[1] / "scripts/install-key-overlay.py"
SPEC = importlib.util.spec_from_file_location("key_install", HELPER)
installer = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(installer)
KERNEL = "6.1.99-rk3576"
CONFIG = (
    f"uname_r={KERNEL}\n"
    'cmdline="console=tty1 rootwait rw"\n'
    "enable_uboot_overlays=1\n"
    "#overlay_start\n"
    "dtoverlay=/dtb/overlay/existing-screen.dtbo\n"
    "# an existing comment\n"
    "#overlay_end\n"
    "stressapptest_enable=0\n"
)


class ConfigTests(unittest.TestCase):
    def test_preserves_all_existing_lines_and_is_idempotent(self):
        updated = installer.updated_config(CONFIG, KERNEL)
        self.assertEqual(updated.replace(installer.OVERLAY_LINE + "\n", ""), CONFIG)
        self.assertEqual(installer.updated_config(updated, KERNEL), updated)

    def test_preserves_crlf_and_commented_entry(self):
        text = CONFIG.replace("# an existing comment", "#" + installer.OVERLAY_LINE)
        text = text.replace("\n", "\r\n")
        updated = installer.updated_config(text, KERNEL)
        without_active_entry = "".join(line for line in updated.splitlines(keepends=True)
                                       if line.strip() != installer.OVERLAY_LINE)
        self.assertEqual(without_active_entry, text)
        self.assertEqual(updated.splitlines().count(installer.OVERLAY_LINE), 1)
        self.assertNotIn("\n", updated.replace("\r\n", ""))

    def test_rejects_ambiguous_or_wrong_layout(self):
        bad = [
            CONFIG.replace("#overlay_start\n", ""),
            CONFIG.replace("#overlay_end", "#overlay_start"),
            CONFIG.replace("#overlay_start", "@START@").replace("#overlay_end", "#overlay_start").replace("@START@", "#overlay_end"),
            CONFIG.replace("enable_uboot_overlays=1", "enable_uboot_overlays=0"),
            CONFIG + "enable_uboot_overlays=1\n",
            CONFIG.replace(KERNEL, "different-kernel"),
            CONFIG + installer.OVERLAY_LINE + "\n",
            CONFIG.replace("#overlay_end", (installer.OVERLAY_LINE + "\n") * 2 + "#overlay_end"),
        ]
        for text in bad:
            with self.subTest(text=text), self.assertRaises(ValueError):
                installer.updated_config(text, KERNEL)


class DeploymentTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="car-key-install-test-")
        self.root = Path(self.temporary.name)
        self.config = self.root / "uEnvLubanCat3.txt"
        self.config.write_text(CONFIG)
        self.config.chmod(0o640)
        self.selector = self.root / "uEnv.txt"
        self.selector.symlink_to(self.config.name)
        self.overlays = self.root / "overlay"
        self.overlays.mkdir()
        self.destination = self.overlays / installer.OVERLAY_NAME
        self.source = self.root / "prepared.dtbo"
        # Header-shaped data is sufficient for filesystem tests; DT parsing is tested separately.
        self.source.write_bytes(b"\xd0\x0d\xfe\xed" + b"\0" * 40)
        self.sync_patch = patch.object(installer.os, "sync")
        self.sync_patch.start()  # Only avoid global sync; atomic_write still fsyncs the test files.

    def tearDown(self):
        self.sync_patch.stop()
        self.temporary.cleanup()

    def deploy(self):
        with contextlib.redirect_stdout(io.StringIO()):
            installer.deploy(self.selector.resolve(), self.overlays, self.source, KERNEL)

    def test_backups_symlink_mode_other_overlay_and_repeat(self):
        unrelated = self.overlays / "existing-screen.dtbo"
        unrelated.write_bytes(b"keep me")
        self.deploy()
        self.assertTrue(self.selector.is_symlink())
        self.assertEqual(self.config.stat().st_mode & 0o777, 0o640)
        self.assertEqual(unrelated.read_bytes(), b"keep me")
        self.assertEqual(self.destination.read_bytes(), self.source.read_bytes())
        backups = list(self.root.glob("uEnvLubanCat3.txt.car-key-backup-*"))
        self.assertEqual(len(backups), 1)
        self.assertEqual(backups[0].read_text(), CONFIG)
        before = self.config.stat().st_mtime_ns
        self.deploy()
        self.assertEqual(self.config.stat().st_mtime_ns, before)
        self.assertEqual(len(list(self.root.glob("*.car-key-backup-*"))), 1)

    def test_backups_previous_overlay(self):
        self.destination.write_bytes(b"old blob")
        self.deploy()
        backups = list(self.overlays.glob("*.backup-*"))
        self.assertEqual(len(backups), 1)
        self.assertEqual(backups[0].read_bytes(), b"old blob")

    def test_recovers_even_after_config_replacement(self):
        real_write = installer.atomic_write
        for existing in (False, True):
            with self.subTest(existing=existing):
                self.config.write_text(CONFIG)
                if existing:
                    self.destination.write_bytes(b"previous blob")
                else:
                    self.destination.unlink(missing_ok=True)
                injected = False

                def fail_once_after_replacement(path, data, file_stat=None):
                    nonlocal injected
                    real_write(path, data, file_stat)
                    if path == self.config and not injected:
                        injected = True
                        raise OSError("simulated directory fsync failure")

                with patch.object(installer, "atomic_write", fail_once_after_replacement):
                    with self.assertRaises(OSError):
                        self.deploy()
                self.assertEqual(self.config.read_text(), CONFIG)
                self.assertTrue(self.selector.is_symlink())
                if existing:
                    self.assertEqual(self.destination.read_bytes(), b"previous blob")
                else:
                    self.assertFalse(self.destination.exists())

    def test_refuses_overlay_symlink_without_writing(self):
        self.destination.symlink_to(self.source)
        with self.assertRaises(ValueError):
            self.deploy()
        self.assertEqual(self.config.read_text(), CONFIG)
        self.assertEqual(list(self.root.glob("*.car-key-backup-*")), [])

    def test_refuses_oversized_boot_config(self):
        self.config.write_text(CONFIG + "#" + "x" * 0x8000 + "\n")
        with self.assertRaises(ValueError):
            self.deploy()
        self.assertFalse(self.destination.exists())


if __name__ == "__main__":
    unittest.main()
