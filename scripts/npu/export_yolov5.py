#!/usr/bin/env python3
"""Export the pinned Rockchip YOLOv5n sigmoid-head graph and compare PT/ORT."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

SOURCE_COMMIT = "23a20ef3e5fa648e765e4a70b30edbfd3aaf8c8c"
WEIGHTS_SHA256 = "4f180cf23ba0717ada0badd6c685026d73d48f184d00fc159c2641284b2ac0a3"

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--weights", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--image", type=Path, required=True)
    args = parser.parse_args()
    source = args.source.resolve()
    actual = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    if actual != SOURCE_COMMIT:
        raise ValueError("YOLOv5 source must match pinned Rockchip revision")
    if subprocess.check_output(["git", "-C", str(source), "status", "--porcelain"], text=True).strip():
        raise ValueError("YOLOv5 source checkout must be clean")
    if hashlib.sha256(args.weights.read_bytes()).hexdigest() != WEIGHTS_SHA256:
        raise ValueError("unexpected official v6.2 YOLOv5n weights checksum")
    os.environ["RKNN_model_hack"] = "npu_2"  # Generic three sigmoid heads, no SoC conversion here.
    os.environ["YOLOv5_AUTOINSTALL"] = "false"
    sys.path.insert(0, str(source))
    import cv2
    import numpy as np
    import onnx
    import onnxruntime as ort
    import torch
    from models.experimental import attempt_load

    torch.set_num_threads(2)
    model = attempt_load(str(args.weights.resolve()), device=torch.device("cpu")).eval()
    image = cv2.imread(str(args.image))
    if image is None:
        raise ValueError("unreadable reference image")
    h, w = image.shape[:2]
    ratio = min(640 / w, 640 / h)
    rw, rh = int(np.floor(w * ratio + .5)), int(np.floor(h * ratio + .5))
    left, top = (640 - rw) // 2, (640 - rh) // 2
    canvas = np.zeros((640, 640, 3), dtype=np.uint8)
    canvas[top:top + rh, left:left + rw] = cv2.resize(image, (rw, rh))
    rgb = cv2.cvtColor(canvas, cv2.COLOR_BGR2RGB)
    data = np.ascontiguousarray(rgb.transpose(2, 0, 1)[None]).astype(np.float32) / 255
    tensor = torch.from_numpy(data)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with torch.no_grad():
        expected = [v.numpy() for v in model(tensor)]
        torch.onnx.export(model, tensor, str(args.output), opset_version=12,
                          input_names=["images"], output_names=["head_s8", "head_s16", "head_s32"],
                          do_constant_folding=True)
    graph = onnx.load(str(args.output))
    onnx.checker.check_model(graph)
    config = ort.SessionOptions()
    config.intra_op_num_threads = 2
    config.inter_op_num_threads = 1
    session = ort.InferenceSession(str(args.output), sess_options=config, providers=["CPUExecutionProvider"])
    actual_outputs = session.run(None, {session.get_inputs()[0].name: data})
    if len(actual_outputs) != 3:
        raise ValueError("expected three optimized heads")
    errors = []
    for stride, pt, output in zip((8, 16, 32), expected, actual_outputs):
        if output.shape != (1, 255, 640 // stride, 640 // stride):
            raise ValueError("unexpected head shape")
        np.testing.assert_allclose(pt, output, atol=1e-4, rtol=1e-3)
        errors.append({"stride": stride, "shape": list(output.shape),
                       "max_abs": float(np.max(np.abs(pt - output))),
                       "mean_abs": float(np.mean(np.abs(pt - output)))})
    report = {"source_commit": SOURCE_COMMIT, "weights_sha256": WEIGHTS_SHA256,
              "onnx_sha256": hashlib.sha256(args.output.read_bytes()).hexdigest(),
              "torch": torch.__version__, "onnx": onnx.__version__, "onnxruntime": ort.__version__,
              "reference_image_sha256": hashlib.sha256(args.image.read_bytes()).hexdigest(),
              "comparison": "adapted PyTorch sigmoid heads versus ONNX Runtime, one fixed image",
              "head_errors": errors, "preprocessing": "RGB, black letterbox, NCHW float32 /255", "opset": 12}
    args.output.with_suffix(".export.json").write_text(json.dumps(report, indent=2) + "\n")
    np.savez_compressed(args.output.with_suffix(".reference.npz"), **dict(zip(("head_s8", "head_s16", "head_s32"), actual_outputs)))
    print(json.dumps(report, indent=2))

if __name__ == "__main__":
    main()
