"""Comparison reporting tests use fixture output, never an NPU or real model."""
import argparse
from contextlib import redirect_stderr, redirect_stdout
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest

SOURCE = Path(__file__).resolve().parents[1] / "scripts/npu/compare_models.py"
spec = importlib.util.spec_from_file_location("npu_comparison", SOURCE)
comparison = importlib.util.module_from_spec(spec)
spec.loader.exec_module(comparison)
LOG = "runtime=fixture driver=fixture\nwarmup_requested=10 processed=110 measured=100\n" + "".join(
    f"{name}_ms mean=20.0 p50=19.0 p95=22.0 max=23.0\n" for name in comparison.METRICS) + \
    "detection class=0 name=person confidence=0.8 xyxy=1,2,11,22\ninference=PASS detections=1 output=fixture.jpg\n"

class ParseTests(unittest.TestCase):
    def test_complete_log(self):
        parsed = comparison.parse_log(LOG, 10, 100)
        self.assertEqual(parsed["measured"], 100)
        self.assertEqual(parsed["timing_ms"]["rknn_run"]["mean"], 20.)
        self.assertEqual(len(parsed["detections"]), 1)
    def test_incomplete_repetitions_fail(self):
        with self.assertRaises(comparison.BenchmarkError):
            comparison.parse_log(LOG.replace("measured=100", "measured=2"), 10, 100)
    def test_missing_metrics_fail(self):
        with self.assertRaises(comparison.BenchmarkError):
            comparison.parse_log(LOG.replace("output_get_ms", "unrelated_ms"), 10, 100)
    def test_nonfinite_timing_fails(self):
        with self.assertRaises(comparison.BenchmarkError):
            comparison.parse_log(LOG.replace("mean=20.0", "mean=nan"), 10, 100)
    def test_detection_count_mismatch_fails(self):
        with self.assertRaises(comparison.BenchmarkError):
            comparison.parse_log(LOG.replace("detections=1", "detections=2"), 10, 100)
    def test_matching_is_class_specific_and_one_to_one(self):
        person = {"class": 0, "confidence": .8, "xyxy": [0,0,10,10]}
        truck = {"class": 7, "confidence": .8, "xyxy": [0,0,10,10]}
        result = comparison.compare_detections([person, person], [person, truck])
        self.assertEqual(len(result["matched"]), 1)
        self.assertEqual(len(result["unmatched_fp"]), 1)
        self.assertEqual(len(result["unmatched_int8"]), 1)

class ReportTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        root = Path(self.temp.name)
        self.binary = root / "fixture-detector"
        self.binary.write_text(f"#!{sys.executable}\nimport sys\nfrom pathlib import Path\n"
                               "model=Path(sys.argv[sys.argv.index('--model')+1])\n"
                               "if model.read_bytes()==b'fail': print('fixture failure',file=sys.stderr);sys.exit(7)\n"
                               f"print({LOG!r})\n")
        self.binary.chmod(0o700)
        fp, quant, image = root/"fp.rknn", root/"int8.rknn", root/"image.jpg"
        for path in (fp, quant, image):
            path.write_bytes(b"fixture")
        self.args = argparse.Namespace(binary=self.binary, fp=fp, int8=quant, image=image,
                                       results=root/"results", warmup=10, repeat=100)
    def tearDown(self):
        self.temp.cleanup()
    def report(self):
        return json.loads(next(self.args.results.glob("*/report.json")).read_text())
    def test_four_runs_and_complete_report(self):
        with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            self.assertEqual(comparison.execute(self.args), 0)
        report = self.report()
        self.assertEqual([r["dtype"] for r in report["runs"]], ["fp", "int8", "int8", "fp"])
        self.assertEqual(report["status"], "COMPLETED")
        self.assertEqual(report["summary"]["groups"]["fp"]["run_count"], 2)
        self.assertEqual(report["summary"]["fp_time_divided_by_int8_time"]["rknn_run"], 1.)
        self.assertEqual(len(report["single_image_detection_comparison"]["matched"]), 1)
    def test_underlying_exit_and_failure_logs_are_preserved(self):
        self.args.fp.write_bytes(b"fail")
        with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            with self.assertRaises(comparison.BenchmarkError) as failure:
                comparison.execute(self.args)
        self.assertEqual(failure.exception.status, 7)
        report = self.report()
        self.assertEqual(report["status"], "FAILED")
        self.assertEqual(report["runs"][0]["returncode"], 7)
        self.assertNotIn("summary", report)

if __name__ == "__main__":
    unittest.main()
