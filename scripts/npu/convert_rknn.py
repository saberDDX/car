#!/usr/bin/env python3
"""Convert only the pinned 640px COCO optimized YOLOv5 graph for RK3576."""
import argparse
import hashlib
from importlib.metadata import version
import json
from pathlib import Path
import tempfile

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--onnx", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--dtype", choices=["fp", "i8"], required=True)
    parser.add_argument("--dataset", type=Path)
    args = parser.parse_args()
    if not version("rknn-toolkit2").startswith("2.1.0"):
        raise ValueError("use the pinned RKNN-Toolkit2 2.1.0 environment")
    import onnx
    from rknn.api import RKNN
    model = onnx.load(str(args.onnx))
    onnx.checker.check_model(model)
    dims = lambda value: [d.dim_value for d in value.type.tensor_type.shape.dim]
    if len(model.graph.input) != 1 or dims(model.graph.input[0]) != [1, 3, 640, 640]:
        raise ValueError("unexpected input graph")
    if [dims(v) for v in model.graph.output] != [[1,255,80,80],[1,255,40,40],[1,255,20,20]]:
        raise ValueError("expected Rockchip optimized sigmoid-head outputs")
    images = []
    if args.dtype == "i8":
        if args.dataset is None:
            raise ValueError("INT8 requires a calibration list")
        for line in args.dataset.read_text().splitlines():
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            path = Path(line.strip())
            if not path.is_absolute():
                path = args.dataset.parent / path
            path = path.resolve(strict=True)
            images.append(path)
        if not images:
            raise ValueError("calibration list is empty")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    rknn = RKNN(verbose=False)
    def checked(result, operation):
        if result != 0:
            raise RuntimeError(f"{operation}: RKNN status {result}")
    try:
        checked(rknn.config(target_platform="rk3576", mean_values=[[0,0,0]], std_values=[[255,255,255]]), "config")
        checked(rknn.load_onnx(model=str(args.onnx.resolve())), "load_onnx")
        with tempfile.TemporaryDirectory() as directory:
            dataset = Path(directory) / "calibration.txt"
            dataset.write_text("".join(str(p) + "\n" for p in images))
            checked(rknn.build(do_quantization=args.dtype == "i8",
                               dataset=str(dataset) if images else None), "build")
        checked(rknn.export_rknn(str(args.output.resolve())), "export_rknn")
    finally:
        rknn.release()
    record = {"target": "rk3576", "toolkit": version("rknn-toolkit2"), "dtype": args.dtype,
              "onnx_sha256": digest(args.onnx), "rknn_sha256": digest(args.output),
              "input": "RGB uint8 NHWC at runtime; embedded /255 normalization",
              "calibration": [{"name": p.name, "sha256": digest(p)} for p in images],
              "board_validation": "NOT_RUN"}
    args.output.with_suffix(".json").write_text(json.dumps(record, indent=2) + "\n")
    print(json.dumps(record, indent=2))

if __name__ == "__main__":
    main()
