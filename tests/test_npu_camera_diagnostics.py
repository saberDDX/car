"""Diagnostic orchestration tests use temporary files and mock the camera child."""
import argparse
from contextlib import nullcontext, redirect_stdout, redirect_stderr
import importlib.util
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

SOURCE = Path(__file__).resolve().parents[1] / "scripts/npu/diagnose_camera.py"
spec = importlib.util.spec_from_file_location("camera_diagnostics_launcher", SOURCE)
module = importlib.util.module_from_spec(spec)
with patch.object(sys, "path", [str(SOURCE.parent), *sys.path]):
    spec.loader.exec_module(module)

class DiagnosticsTests(unittest.TestCase):
    def test_previous_result_preserved_and_capture_has_no_model_arguments(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            previous = root / "result-camera.jpg"
            previous.write_bytes(b"original result fixture")
            args = argparse.Namespace(build_directory=root, device="auto")
            commands = []
            def launch(command):
                commands.append(command)
                (Path(command[2]) / "frame-60.png").write_bytes(b"fixture")
                return 0
            with patch.object(module.argparse.ArgumentParser, "parse_args", return_value=args), \
                 patch.object(module.os, "geteuid", return_value=1000), \
                 patch.object(module, "select_camera", return_value="/dev/video-test"), \
                 patch.object(module, "camera_access", return_value=nullcontext()), \
                 patch.object(module, "run_detector", side_effect=launch), \
                 patch.object(module.subprocess, "run", return_value=subprocess.CompletedProcess([],0,"fixture statistics\n","")), \
                 redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
                self.assertEqual(module.main(), 0)
            self.assertEqual(previous.read_bytes(), b"original result fixture")
            self.assertEqual(commands[0][0], str(root / "camera_diagnostics"))
            self.assertNotIn("--model", commands[0])
            latest = root / "camera-diagnostics/latest"
            self.assertTrue(latest.is_symlink())
            self.assertTrue((latest / "frame-60.png").exists())
            self.assertIn("fixture statistics", (latest / "previous-result-inspection.txt").read_text())
    def test_latest_alias_does_not_overwrite_user_file(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            directory = root / "camera-diagnostics"
            directory.mkdir()
            latest = directory / "latest"
            latest.write_text("preserve this file")
            args = argparse.Namespace(build_directory=root, device="auto")
            with patch.object(module.argparse.ArgumentParser, "parse_args", return_value=args), \
                 patch.object(module.os, "geteuid", return_value=1000):
                with self.assertRaisesRegex(RuntimeError, "refusing to overwrite"):
                    module.main()
            self.assertEqual(latest.read_text(), "preserve this file")

if __name__ == "__main__":
    unittest.main()
