#!/usr/bin/env python3
"""Host-only loader tests. The fake library does not implement an NPU."""
from pathlib import Path
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
symbols = ("rknn_init", "rknn_query", "rknn_inputs_set", "rknn_run",
           "rknn_outputs_get", "rknn_outputs_release", "rknn_destroy")
with tempfile.TemporaryDirectory() as temp:
    root = Path(temp)
    for missing in (False, True):
        source = root / "fixture.cpp"
        source.write_text('extern "C" {\n' + ''.join(f"void {name}() {{}}\n" for name in (symbols[:-1] if missing else symbols)) + '}\n')
        library = root / ("missing.so" if missing else "complete.so")
        subprocess.run(["g++", "-shared", "-fPIC", str(source), "-o", str(library)], check=True)
        result = subprocess.run([str(binary), str(library)], capture_output=True, text=True)
        assert result.returncode == (1 if missing else 0), result
        assert "model_inference=NOT_RUN" in result.stdout
        if missing:
            assert "rknn_destroy" in result.stderr
    absent = subprocess.run([str(binary), str(root / "absent.so")], capture_output=True, text=True)
    assert absent.returncode == 1 and "runtime_load=FAIL" in absent.stderr
    usage = subprocess.run([str(binary), "a", "b"], capture_output=True, text=True)
    assert usage.returncode == 2
print("PASS: loader, missing library/symbol, usage and inference-not-run reporting (host fixtures only)")
