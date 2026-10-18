"""Run the actual CLI against ABI fixtures; no I2C hardware is emulated here."""
import csv
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
MONITOR = ROOT / "tools/mpu6050-monitor/mpu6050-monitor"
SAMPLE = struct.Struct("=QhhhhhhhH")


class MonitorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        subprocess.run(["make", "-C", str(MONITOR.parent)], check=True, capture_output=True)

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="car-mpu-monitor-")
        self.root = Path(self.temporary.name)
        self.device = self.root / "samples.bin"
        self.csv = self.root / "samples.csv"

    def tearDown(self):
        self.temporary.cleanup()

    def run_monitor(self, records, *extra):
        self.device.write_bytes(records)
        return subprocess.run([str(MONITOR), "-d", str(self.device), "-s", "0.03",
                               "-r", "100", "-q", *extra], capture_output=True, text=True, timeout=5)

    def fixture(self, **changes):
        values = dict(ax=-16384, ay=0, az=16384, temp=-340, gx=-131, gy=262, gz=-32768,
                      reserved=0)
        values.update(changes)
        # Above 2^53: catch timestamp corruption through double formatting.
        return b"".join(SAMPLE.pack(9007199254740993 + n * 10000000, *values.values())
                        for n in range(100))

    def test_negative_units_temperature_norm_and_exact_64bit_csv(self):
        result = self.run_monitor(self.fixture(), "-o", str(self.csv))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("errors=0", result.stderr)
        self.assertIn("actual_hz=100.000", result.stderr)
        with self.csv.open() as handle:
            rows = list(csv.DictReader(handle))
        self.assertGreater(len(rows), 1)
        row = rows[0]
        self.assertEqual(row["timestamp_ns"], "9007199254740993")
        self.assertEqual(row["gz_raw"], "-32768")
        self.assertAlmostEqual(float(row["ax_g"]), -1)
        self.assertAlmostEqual(float(row["gz_dps"]), -32768 / 131, places=5)
        self.assertAlmostEqual(float(row["gy_dps"]), 2)
        self.assertAlmostEqual(float(row["chip_temp_c"]), 35.53)
        self.assertAlmostEqual(float(row["accel_norm_g"]), 2 ** 0.5, places=5)

    def test_rejects_eof_and_partial_sample_instead_of_reusing_stale_data(self):
        for records in (b"", b"\0" * 23):
            with self.subTest(length=len(records)):
                result = self.run_monitor(records)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Incomplete sample", result.stderr)
                self.assertIn("Samples=0 errors=1", result.stderr)

    def test_rejects_repeated_timestamp_and_reserved_field(self):
        first = self.fixture()[:SAMPLE.size]
        for records in (first * 100, self.fixture(reserved=1)):
            with self.subTest(records=records[:SAMPLE.size]):
                result = self.run_monitor(records)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Invalid ABI sample", result.stderr)

    def test_rejects_invalid_rates_durations_and_trailing_arguments(self):
        for extra in (("-r", "nan"), ("-r", "0"), ("-r", "101"), ("-s", "inf"),
                      ("-s", "-1"), ("extra",)):
            with self.subTest(extra=extra):
                result = self.run_monitor(self.fixture(), *extra)
                self.assertNotEqual(result.returncode, 0)

    def test_does_not_overwrite_existing_csv(self):
        self.csv.write_text("previous results\n")
        result = self.run_monitor(self.fixture(), "-o", str(self.csv))
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.csv.read_text(), "previous results\n")

    def test_open_failure_and_csv_write_failure_are_not_success(self):
        result = subprocess.run([str(MONITOR), "-d", str(self.root / "absent")],
                                capture_output=True, text=True, timeout=5)
        self.assertNotEqual(result.returncode, 0)
        result = self.run_monitor(self.fixture(), "-o", str(self.root / "missing/out.csv"))
        self.assertNotEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main()
