"""Exercise the PowerShell verifier core with synthetic images, without Windows UI.

Set BSP_PWSH to a portable pwsh executable. Only the temporary test copy gets
fixture hashes; the shipped script always keeps the recorded real board hashes.
"""
import gzip
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
PWSH = os.environ.get("BSP_PWSH") or shutil.which("pwsh")
SIZE = 134217728


@unittest.skipUnless(PWSH, "Set BSP_PWSH or install pwsh to exercise the PowerShell verifier")
class WindowsReadbackTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="bsp-pwsh-", dir=ROOT / "build/bsp")
        cls.base = Path(cls.temp.name)
        cls.prefix = bytearray(4096)
        for offset, value in ((1028, 32768), (1044, 0), (1048, 2), (1056, 32768), (1064, 8192)):
            struct.pack_into("<I", cls.prefix, offset, value)
        struct.pack_into("<H", cls.prefix, 1080, 0xEF53)
        cls.prefix[1128:1144] = bytes(range(16))
        cls.raw = cls.base / "original backup.img"
        cls.write_image(cls.raw, cls.prefix)
        cls.gz = cls.base / "original backup.img.gz"
        cls.compress(cls.raw, cls.gz, 1)
        raw_hash = hashlib.sha256(cls.raw.read_bytes()).hexdigest()
        gzip_hash = hashlib.sha256(cls.gz.read_bytes()).hexdigest()
        code = (ROOT / "scripts/check-bsp-boot-readback.ps1").read_text()
        code = code.replace("0b4b656ab67b9365fe7b5e3de90940f45f8e95c293ad26823815a09a4c133444", raw_hash)
        code = code.replace("6fa3cc6ca8ce04684c74d66897a9345e9e5333e101553eea84ec157fa1d9679a", gzip_hash)
        cls.script = cls.base / "fixture check.ps1"
        cls.script.write_text(code)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    @staticmethod
    def write_image(path, prefix, size=SIZE):
        with path.open("wb") as stream:
            stream.write(prefix)
            stream.truncate(size)

    @staticmethod
    def compress(source, target, mtime):
        with source.open("rb") as src, target.open("wb") as dst:
            with gzip.GzipFile(filename="", mode="wb", fileobj=dst, mtime=mtime) as compressed:
                shutil.copyfileobj(src, compressed)

    def setUp(self):
        self.case = Path(tempfile.mkdtemp(dir=self.base))
        self.export = self.case / "new export.img"
        self.write_image(self.export, self.prefix)

    def run_check(self, backup):
        env = os.environ.copy()
        for name in ("CACHE", "CONFIG", "DATA"):
            env[f"XDG_{name}_HOME"] = str(self.base / name.lower())
        return subprocess.run([PWSH, "-NoProfile", "-File", str(self.script),
                               "-ExportImagePath", str(self.export), "-BackupImagePath", str(backup)],
                              capture_output=True, text=True, timeout=30, env=env)

    def report(self, result):
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        reports = list(self.case.glob("boot-readback-check-*.json"))
        self.assertEqual(len(reports), 1)
        return json.loads(reports[0].read_text(encoding="utf-8-sig"))

    def test_original_gzip_matches(self):
        report = self.report(self.run_check(self.gz))
        self.assertTrue(report["original_gzip_hash_matches_record"])
        self.assertTrue(report["raw_hash_matches_original_backup"])

    def test_recompressed_same_snapshot_is_accepted(self):
        gz = self.case / "recompressed.gz"
        self.compress(self.raw, gz, 2)
        report = self.report(self.run_check(gz))
        self.assertFalse(report["original_gzip_hash_matches_record"])
        self.assertTrue(report["raw_hash_matches_original_backup"])

    def test_original_raw_is_accepted(self):
        report = self.report(self.run_check(self.raw))
        self.assertEqual(report["original_backup_encoding"], "raw")

    def test_changed_backup_rejected_with_diagnostics(self):
        wrong = self.case / "wrong backup.img"
        prefix = self.prefix.copy()
        prefix[-1] ^= 1
        self.write_image(wrong, prefix)
        result = self.run_check(wrong)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Original raw backup does not match", result.stderr)
        self.assertIn(str(wrong), result.stdout)
        self.assertIn("Original backup raw SHA256:", result.stdout)
        self.assertFalse(list(self.case.glob("boot-readback-check-*.json")))

    def test_changed_mount_time_is_identity_only_match(self):
        prefix = self.prefix.copy()
        struct.pack_into("<I", prefix, 1068, 123456789)
        self.write_image(self.export, prefix)
        report = self.report(self.run_check(self.gz))
        self.assertFalse(report["raw_hash_matches_original_backup"])
        self.assertTrue(report["filesystem_uuid_matches_backup"])

    def test_wrong_export_magic_and_uuid_rejected(self):
        for offset, error in ((1080, "Missing ext2"), (1128, "Filesystem UUID differs")):
            prefix = self.prefix.copy()
            prefix[offset] ^= 1
            self.write_image(self.export, prefix)
            result = self.run_check(self.gz)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(error, result.stderr)

    def test_gpt_sized_export_rejected(self):
        self.write_image(self.export, self.prefix, 20480)
        result = self.run_check(self.gz)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Wrong export size", result.stderr)

    def test_oversized_compressed_backup_rejected(self):
        raw = self.case / "too long.img"
        gz = self.case / "too long.gz"
        self.write_image(raw, self.prefix, SIZE + 1)
        self.compress(raw, gz, 1)
        result = self.run_check(gz)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("expands beyond 128 MiB", result.stderr)


if __name__ == "__main__":
    unittest.main()
