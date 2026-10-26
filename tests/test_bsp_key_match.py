"""Controlled matching-fault state machine and restoration; no sysfs writes."""
import importlib.util
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("bsp_match", ROOT / "scripts/test-bsp-key-match.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class FakeKey:
    def __init__(self):
        self.value = ""
        self.bound = True
        self.probes = 0
        self.unbinds = 0
        self.fail_set = False
        self.fail_unbind = False
        self.fail_probe = False
        self.interrupt_unbind = False

    def override(self):
        return self.value

    def set_override(self, value):
        self.value = value
        if self.fail_set:
            self.fail_set = False
            raise OSError("after setting override")

    def unbind(self):
        self.unbinds += 1
        self.bound = False
        if self.interrupt_unbind:
            raise KeyboardInterrupt()
        if self.fail_unbind:
            raise OSError("after unbinding")

    def probe(self):
        self.probes += 1
        if self.fail_probe:
            self.fail_probe = False
            raise OSError("failed first rescan")
        self.bound = self.value in ("", "car-gpio-key")

    def snapshot(self):
        return {"device_present": True, "compatible": MODULE.COMPATIBLE, "module_loaded": True,
                "driver": str(MODULE.DRIVER) if self.bound else None,
                "driver_override": self.value, "events": ["/dev/input/event6"] if self.bound else []}

    def wait_ready(self):
        if not self.bound:
            raise RuntimeError("not recovered")
        return self.snapshot()


class MatchTests(unittest.TestCase):
    def run_exercise(self, device):
        return MODULE.exercise(device, "car-intentionally-unmatched-fixture", emit=lambda line: None)

    def test_match_failure_and_recovery_without_changing_compatible(self):
        device = FakeKey()
        result = self.run_exercise(device)
        self.assertTrue(result["fault"]["device_present"])
        self.assertTrue(result["fault"]["module_loaded"])
        self.assertEqual(result["fault"]["compatible"], MODULE.COMPATIBLE)
        self.assertIsNone(result["fault"]["driver"])
        self.assertEqual(result["fault"]["events"], [])
        self.assertEqual(result["before"], result["restored"])
        self.assertEqual(device.probes, 2)

    def test_preexisting_override_is_not_changed(self):
        device = FakeKey()
        device.value = "other-owner-driver"
        with self.assertRaises(ValueError):
            self.run_exercise(device)
        self.assertEqual(device.value, "other-owner-driver")
        self.assertEqual(device.unbinds, 0)

    def test_set_failure_after_write_still_clears_override(self):
        device = FakeKey()
        device.fail_set = True
        with self.assertRaises(OSError):
            self.run_exercise(device)
        self.assertEqual(device.value, "")
        self.assertTrue(device.bound)

    def test_unbind_failure_rebinds_and_preserves_failure(self):
        device = FakeKey()
        device.fail_unbind = True
        with self.assertRaises(OSError):
            self.run_exercise(device)
        self.assertEqual(device.value, "")
        self.assertTrue(device.bound)

    def test_rescan_failure_restores_but_does_not_report_pass(self):
        device = FakeKey()
        device.fail_probe = True
        with self.assertRaises(OSError):
            self.run_exercise(device)
        self.assertEqual(device.value, "")
        self.assertTrue(device.bound)

    def test_keyboard_interrupt_runs_restoration(self):
        device = FakeKey()
        device.interrupt_unbind = True
        with self.assertRaises(KeyboardInterrupt):
            self.run_exercise(device)
        self.assertEqual(device.value, "")
        self.assertTrue(device.bound)

    def test_another_owners_concurrent_override_is_not_clobbered(self):
        device = FakeKey()

        def takeover():
            device.value = "another-owner"

        device.probe = takeover
        with self.assertRaisesRegex(RuntimeError, "Another owner"):
            self.run_exercise(device)
        self.assertEqual(device.value, "another-owner")

    def test_cloud_main_refused_before_board_operations(self):
        result = subprocess.run(["python3", str(ROOT / "scripts/test-bsp-key-match.py")],
                                capture_output=True, text=True, timeout=10)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("STOP:", result.stderr)


if __name__ == "__main__":
    unittest.main()
