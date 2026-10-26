"""Check serialization and refusal paths for the experiment boot artifact."""
import importlib.util
from pathlib import Path
import stat
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location(
    "bsp_initrd", Path(__file__).resolve().parents[1] / "scripts/prepare-bsp-initrd.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class InitrdTests(unittest.TestCase):
    def setUp(self):
        self.fields = (0x27051956, 0, 1733970492, 0, 0, 0, 0, 5, 2, 3, 1, bytes(32))
        self.archive = b"".join([
            MODULE.record("init", stat.S_IFREG | 0o755, b"#!/bin/sh\nexec run-init\n", 1),
            MODULE.record("lib", stat.S_IFLNK | 0o777, b"usr/lib", 2),
            MODULE.record("usr/lib/modules", stat.S_IFDIR | 0o755, b"", 3),
            MODULE.record("usr/lib/modules/6.1.84/" + MODULE.EARLY_MODULE,
                          stat.S_IFREG | 0o644, b"old module", 4),
            MODULE.record("TRAILER!!!", 0, b"", 5),
        ])

    def test_crc_and_compression_roundtrip(self):
        image = MODULE.wrap_image(self.fields, self.archive)
        fields, decoded = MODULE.unpack_image(image)
        self.assertEqual(decoded, self.archive)
        self.assertEqual(fields[8:11], (2, 3, 1))
        self.assertEqual(MODULE.parse_archive(decoded)[0]["data"], b"#!/bin/sh\nexec run-init\n")

    def test_rejects_corrupt_header_and_payload(self):
        image = MODULE.wrap_image(self.fields, self.archive)
        for index in (10, len(image) - 1):
            corrupted = bytearray(image)
            corrupted[index] ^= 1
            with self.assertRaisesRegex(ValueError, "CRC"):
                MODULE.unpack_image(corrupted)

    def test_rejects_archive_traversal_truncation_and_hidden_data(self):
        cases = [MODULE.record("../etc/init", stat.S_IFREG | 0o755, b"x", 1),
                 self.archive[:115], self.archive + b"hidden"]
        for archive in cases:
            with self.assertRaises(ValueError):
                MODULE.parse_archive(archive)

    def test_decompression_is_bounded(self):
        image = MODULE.wrap_image(self.fields, self.archive)
        with patch.object(MODULE, "MAX_ARCHIVE", 16):
            with self.assertRaisesRegex(ValueError, "size limit"):
                MODULE.unpack_image(image)

    def test_replaces_only_module_subtree(self):
        before = MODULE.parse_archive(self.archive)
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp) / MODULE.RELEASE
            module = root / MODULE.EARLY_MODULE
            module.parent.mkdir(parents=True)
            module.write_bytes(b"new module")
            rebuilt = MODULE.replace_modules(before, root)
        after = MODULE.parse_archive(rebuilt)
        old = {entry["name"]: entry for entry in before}
        new = {entry["name"]: entry for entry in after}
        for name in ("init", "lib", "usr/lib/modules"):
            self.assertEqual(old[name]["raw"], new[name]["raw"])
        self.assertNotIn("usr/lib/modules/6.1.84/" + MODULE.EARLY_MODULE, new)
        self.assertEqual(new["usr/lib/modules/" + MODULE.RELEASE + "/" + MODULE.EARLY_MODULE]["data"], b"new module")


if __name__ == "__main__":
    unittest.main()
