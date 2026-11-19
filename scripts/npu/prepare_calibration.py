#!/usr/bin/env python3
"""Create a small black-letterboxed calibration set without changing originals."""
import argparse
import hashlib
import json
from pathlib import Path
import cv2
import numpy as np

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--source-list", type=Path, required=True)
parser.add_argument("--output-directory", type=Path, required=True)
args = parser.parse_args()
args.output_directory.mkdir(parents=True, exist_ok=True)
records, outputs = [], []
for line in args.source_list.read_text().splitlines():
    if not line.strip() or line.lstrip().startswith("#"):
        continue
    source = (args.source_list.parent / line.strip()).resolve(strict=True)
    image = cv2.imread(str(source))
    if image is None:
        raise ValueError(f"unreadable calibration image: {source}")
    h, w = image.shape[:2]
    ratio = min(640 / w, 640 / h)
    rw, rh = int(np.floor(w * ratio + .5)), int(np.floor(h * ratio + .5))
    left, top = (640 - rw) // 2, (640 - rh) // 2
    canvas = np.zeros((640, 640, 3), np.uint8)
    canvas[top:top + rh, left:left + rw] = cv2.resize(image, (rw, rh))
    output = args.output_directory / f"{len(outputs):03d}-{source.stem}.png"
    if not cv2.imwrite(str(output), canvas):
        raise RuntimeError(f"cannot write {output}")
    outputs.append(output.resolve())
    records.append({"name": source.name, "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                    "prepared_sha256": hashlib.sha256(output.read_bytes()).hexdigest()})
if not outputs:
    raise ValueError("no calibration images")
(args.output_directory / "calibration.txt").write_text("".join(str(p) + "\n" for p in outputs))
(args.output_directory / "sources.json").write_text(json.dumps(records, indent=2) + "\n")
print(f"Prepared {len(outputs)} images; this small demonstration set is not a full accuracy evaluation.")
