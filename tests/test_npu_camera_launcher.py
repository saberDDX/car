"""Host tests mock privileged commands; no device ACL or camera is changed."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import Mock, patch

SOURCE = Path(__file__).resolve().parents[1] / "scripts/npu/run_camera.py"
spec = importlib.util.spec_from_file_location("npu_camera_launcher", SOURCE)
launcher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(launcher)

class CameraACLTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.directory = Path(self.temp.name) / "acl"
        self.directory.mkdir()
        self.calls = []
        self.patches = [patch.object(launcher, "ensure_sudo"),
                        patch.object(launcher.shutil, "which", return_value="/fake/tool"),
                        patch.object(launcher.tempfile, "mkdtemp", return_value=str(self.directory)),
                        patch.object(launcher.os, "getuid", return_value=1000),
                        patch.object(launcher, "device_identity", return_value=(1,2,3)),
                        patch.object(launcher, "root_command", side_effect=self.root)]
        for item in self.patches:
            item.start()
    def tearDown(self):
        for item in reversed(self.patches):
            item.stop()
        self.temp.cleanup()
    def root(self, arguments, **kwargs):
        self.calls.append(arguments)
        if "stdout" in kwargs:
            kwargs["stdout"].write("# file: /dev/video-test\nuser::rw-\n")
        return subprocess.CompletedProcess(arguments, 0)
    def restored(self):
        return any(any(str(value).startswith("--restore=") for value in call) for call in self.calls)
    def test_existing_access_needs_no_sudo(self):
        with patch.object(launcher.os, "access", return_value=True):
            with launcher.camera_access("/dev/video-test"):
                pass
        self.assertEqual(self.calls, [])
        launcher.ensure_sudo.assert_not_called()
    def test_grant_restores_original_and_removes_backup(self):
        with patch.object(launcher.os, "access", side_effect=[False, True]):
            with launcher.camera_access("/dev/video-test"):
                self.assertTrue((self.directory / "camera.acl").exists())
        self.assertIn(["setfacl", "-m", "u:1000:rw", "--", "/dev/video-test"], self.calls)
        self.assertTrue(self.restored())
        self.assertFalse(self.directory.exists())
    def test_detector_exception_still_restores(self):
        with patch.object(launcher.os, "access", side_effect=[False, True]):
            with self.assertRaisesRegex(RuntimeError, "inference failed"):
                with launcher.camera_access("/dev/video-test"):
                    raise RuntimeError("inference failed")
        self.assertTrue(self.restored())
    def test_grant_failure_still_restores(self):
        def failure(arguments, **kwargs):
            if "-m" in arguments:
                raise subprocess.CalledProcessError(1, arguments)
            return self.root(arguments, **kwargs)
        with patch.object(launcher.os, "access", return_value=False), patch.object(launcher, "root_command", side_effect=failure):
            with self.assertRaises(subprocess.CalledProcessError):
                with launcher.camera_access("/dev/video-test"):
                    self.fail("must not run detector")
        self.assertTrue(self.restored())
    def test_replaced_device_keeps_backup_without_wrong_restore(self):
        with patch.object(launcher.os, "access", side_effect=[False, True]), \
             patch.object(launcher, "device_identity", side_effect=[(1,2,3),(1,2,3),(1,2,3),(1,9,3)]):
            with self.assertRaisesRegex(RuntimeError, "identity changed"):
                with launcher.camera_access("/dev/video-test"):
                    pass
        self.assertFalse(self.restored())
        self.assertTrue((self.directory / "camera.acl").exists())
    def test_restore_failure_retains_backup(self):
        def failure(arguments, **kwargs):
            if any(str(arg).startswith("--restore=") for arg in arguments):
                raise subprocess.CalledProcessError(1, arguments)
            return self.root(arguments, **kwargs)
        with patch.object(launcher.os, "access", side_effect=[False, True]), patch.object(launcher, "root_command", side_effect=failure):
            with self.assertRaisesRegex(RuntimeError, "Original ACL retained"):
                with launcher.camera_access("/dev/video-test"):
                    pass
        self.assertTrue((self.directory / "camera.acl").exists())

class ProbeTests(unittest.TestCase):
    def test_permission_denied_only_escalates_metadata_probe(self):
        denied = subprocess.CompletedProcess([], 1, "", "camera_probe open_errno=13 error=Permission denied\n")
        found = subprocess.CompletedProcess([], 0, "/dev/video-test\n", "driver=uvcvideo formats=YUYV\n")
        with patch.object(launcher.subprocess, "run", return_value=denied), \
             patch.object(launcher, "ensure_sudo") as auth, \
             patch.object(launcher, "root_command", return_value=found) as root, \
             patch.object(launcher, "resolve_device", return_value="/dev/video-test"):
            self.assertEqual(launcher.select_camera(Path("/app/detect_image"), "auto"), "/dev/video-test")
        auth.assert_called_once()
        self.assertEqual(root.call_args.args[0], ["/app/detect_image", "--find-camera"])
    def test_format_failure_does_not_request_sudo(self):
        rejected = subprocess.CompletedProcess([], 1, "", "camera_probe rejected=missing_YUYV\n")
        with patch.object(launcher.subprocess, "run", return_value=rejected), patch.object(launcher, "ensure_sudo") as auth:
            with self.assertRaisesRegex(RuntimeError, "discovery failed"):
                launcher.select_camera(Path("/app/detect_image"), "auto")
        auth.assert_not_called()

class ProcessTests(unittest.TestCase):
    def test_detector_runs_without_sudo(self):
        child = Mock()
        child.wait.return_value = 0
        child.poll.return_value = 0
        command = ["/app/detect_image", "--camera", "/dev/video-test"]
        with patch.object(launcher.subprocess, "Popen", return_value=child) as start:
            self.assertEqual(launcher.run_detector(command), 0)
        start.assert_called_once_with(command)
    def test_launcher_rejects_whole_program_as_root(self):
        with patch.object(launcher.argparse.ArgumentParser, "parse_args"), \
             patch.object(launcher.os, "geteuid", return_value=0):
            with self.assertRaisesRegex(RuntimeError, "without sudo"):
                launcher.main()

if __name__ == "__main__":
    unittest.main()
