#!/usr/bin/env python3
"""Board-only paired FP/INT8 image measurements. No ground-truth accuracy claim."""
import argparse
from datetime import datetime, timedelta, timezone
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
import sys
import tempfile

METRICS = ("preprocess", "input_set", "rknn_run", "output_get", "postprocess", "processing")

class BenchmarkError(RuntimeError):
    def __init__(self, message, status=1):
        super().__init__(message)
        self.status = status

def parse_log(text, warmup, repeat):
    count = re.search(r"warmup_requested=(\d+) processed=(\d+) measured=(\d+)", text)
    if not count or tuple(map(int, count.groups())) != (warmup, warmup + repeat, repeat):
        raise BenchmarkError("requested warmup/repetitions did not complete")
    version = re.search(r"^runtime=(.*?) driver=(.*?)$", text, re.M)
    if not version:
        raise BenchmarkError("missing actual runtime/driver query")
    metrics = {}
    for match in re.finditer(r"^([a-z_]+)_ms mean=(\S+) p50=(\S+) p95=(\S+) max=(\S+)$", text, re.M):
        values = list(map(float, match.groups()[1:]))
        if not all(math.isfinite(v) and v >= 0 for v in values):
            raise BenchmarkError("invalid timing value")
        metrics[match[1]] = dict(zip(("mean", "p50", "p95", "max"), values))
    if any(key not in metrics for key in METRICS):
        raise BenchmarkError("missing a required timing segment")
    detections = []
    for match in re.finditer(r"^detection class=(\d+) name=(\S+) confidence=(\S+) xyxy=(\S+)$", text, re.M):
        confidence = float(match[3])
        box = list(map(float, match[4].split(",")))
        if len(box) != 4 or not all(math.isfinite(v) for v in [confidence, *box]) or not 0 <= confidence <= 1:
            raise BenchmarkError("invalid detection values")
        detections.append({"class": int(match[1]), "name": match[2], "confidence": confidence, "xyxy": box})
    passed = re.search(r"^inference=PASS detections=(\d+) ", text, re.M)
    if not passed or int(passed[1]) != len(detections):
        raise BenchmarkError("inference or detection-count check failed")
    return {"runtime": version[1], "driver": version[2], "measured": repeat,
            "timing_ms": metrics, "detections": detections}

def iou(a, b):
    intersection = max(0, min(a[2], b[2]) - max(a[0], b[0])) * max(0, min(a[3], b[3]) - max(a[1], b[1]))
    area = max(0, a[2]-a[0])*max(0, a[3]-a[1]) + max(0, b[2]-b[0])*max(0, b[3]-b[1]) - intersection
    return intersection / area if area > 0 else 0

def compare_detections(fp, quantized):
    candidates = []
    for i, a in enumerate(fp):
        for j, b in enumerate(quantized):
            overlap = iou(a["xyxy"], b["xyxy"])
            if a["class"] == b["class"] and overlap >= .5:
                candidates.append((overlap, i, j))
    used_fp, used_int8, matched = set(), set(), []
    for overlap, i, j in sorted(candidates, reverse=True):
        if i in used_fp or j in used_int8:
            continue
        used_fp.add(i)
        used_int8.add(j)
        matched.append({"class": fp[i]["class"], "fp_index": i, "int8_index": j, "iou": overlap,
                        "confidence_delta_int8_minus_fp": quantized[j]["confidence"] - fp[i]["confidence"],
                        "max_box_abs_delta_px": max(abs(a-b) for a, b in zip(fp[i]["xyxy"], quantized[j]["xyxy"]))})
    return {"method": "greedy same-class IoU >= 0.5; neither model is ground truth",
            "fp_count": len(fp), "int8_count": len(quantized), "matched": matched,
            "unmatched_fp": [fp[i] for i in range(len(fp)) if i not in used_fp],
            "unmatched_int8": [quantized[i] for i in range(len(quantized)) if i not in used_int8]}

def summarize(runs):
    groups = {}
    for dtype in ("fp", "int8"):
        selected = [run for run in runs if run["dtype"] == dtype]
        groups[dtype] = {"run_count": len(selected),
                        "mean_of_run_means_ms": {name: sum(r["timing_ms"][name]["mean"] for r in selected)/len(selected) for name in METRICS},
                        "detection_counts": [len(r["detections"]) for r in selected]}
    ratios = {}
    for name in ("rknn_run", "processing"):
        denominator = groups["int8"]["mean_of_run_means_ms"][name]
        ratios[name] = groups["fp"]["mean_of_run_means_ms"][name]/denominator if denominator else None
    return {"groups": groups, "fp_time_divided_by_int8_time": ratios,
            "aggregation": "arithmetic mean of equal-sized run means; P50/P95 remain per run, not pooled"}

def execute(args):
    args.results.mkdir(parents=True, exist_ok=True)
    directory = Path(tempfile.mkdtemp(prefix="comparison-", dir=args.results))
    output = directory / "report.json"
    report = {"status": "RUNNING", "timestamp": datetime.now(timezone(timedelta(hours=8))).isoformat(),
              "scope": "one fixed image; not mAP, stable camera FPS or full dataset accuracy",
              "order": ["fp", "int8", "int8", "fp"], "warmup_per_run": args.warmup,
              "measured_per_run": args.repeat, "confidence": .25, "nms": .45,
              "classes": [0,2,3,5,7], "runs": []}
    def save():
        output.write_text(json.dumps(report, indent=2, allow_nan=False) + "\n")
    try:
        models = {"fp": args.fp.resolve(strict=True), "int8": args.int8.resolve(strict=True)}
        image = args.image.resolve(strict=True)
        report["image_sha256"] = hashlib.sha256(image.read_bytes()).hexdigest()
        report["models"] = {dtype: {"name": model.name, "bytes": model.stat().st_size,
                                    "sha256": hashlib.sha256(model.read_bytes()).hexdigest()} for dtype, model in models.items()}
        for index, dtype in enumerate(report["order"]):
            prefix = directory / f"{index+1}-{dtype}"
            log = prefix.with_suffix(".stdout.txt")
            error_log = prefix.with_suffix(".stderr.txt")
            row = {"dtype": dtype, "stdout": log.name, "stderr": error_log.name, "status": "RUNNING"}
            report["runs"].append(row)
            save()
            command = [str(args.binary.resolve(strict=True)), "--model", str(models[dtype]), "--image", str(image),
                       "--output", str(prefix.with_suffix(".jpg")), "--warmup", str(args.warmup), "--repeat", str(args.repeat),
                       "--confidence", "0.25", "--nms", "0.45", "--classes", "0,2,3,5,7"]
            with log.open("w") as stdout, error_log.open("w") as stderr:
                process = subprocess.run(command, stdout=stdout, stderr=stderr, timeout=120)
            row["returncode"] = process.returncode
            if process.returncode:
                print(log.read_text()[-5000:], file=sys.stderr)
                print(error_log.read_text()[-5000:], file=sys.stderr)
                raise BenchmarkError(f"{dtype} run failed; logs retained in {directory}", process.returncode if process.returncode > 0 else 128-process.returncode)
            row.update(parse_log(log.read_text(), args.warmup, args.repeat))
            row["status"] = "COMPLETED"
            print(f"run={index+1} model={dtype} rknn_run_mean_ms={row['timing_ms']['rknn_run']['mean']:.3f} processing_mean_ms={row['timing_ms']['processing']['mean']:.3f} detections={len(row['detections'])}", flush=True)
        if len({(row["runtime"], row["driver"]) for row in report["runs"]}) != 1:
            raise BenchmarkError("runtime/driver changed between runs; results are not comparable")
        report["summary"] = summarize(report["runs"])
        report["single_image_detection_comparison"] = compare_detections(report["runs"][0]["detections"], report["runs"][1]["detections"])
        report["status"] = "COMPLETED"
        save()
        summary = report["summary"]
        print("model   rknn_run_mean_ms   processing_mean_ms   model_bytes")
        for dtype in ("fp", "int8"):
            mean = summary["groups"][dtype]["mean_of_run_means_ms"]
            print(f"{dtype:5}   {mean['rknn_run']:16.3f}   {mean['processing']:18.3f}   {report['models'][dtype]['bytes']}")
        difference = report["single_image_detection_comparison"]
        print(f"single_image matched={len(difference['matched'])} unmatched_fp={len(difference['unmatched_fp'])} unmatched_int8={len(difference['unmatched_int8'])}; not an accuracy score")
        print(f"report={output}")
        return 0
    except BaseException as error:
        report["status"] = "FAILED"
        report["error"] = str(error)
        save()
        print(f"Incomplete comparison saved at {output}", file=sys.stderr)
        raise

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("binary", "fp", "int8", "image", "results"):
        parser.add_argument("--"+name, type=Path, required=True)
    parser.add_argument("--warmup", type=int, default=10)
    parser.add_argument("--repeat", type=int, default=100)
    args = parser.parse_args()
    if not 0 <= args.warmup <= 1000 or not 1 <= args.repeat <= 10000:
        parser.error("warmup must be 0..1000; repeat must be 1..10000")
    return execute(args)

if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
    except BenchmarkError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(error.status)
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(124 if isinstance(error, subprocess.TimeoutExpired) else 1)
