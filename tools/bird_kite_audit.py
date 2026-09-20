#!/usr/bin/env python3
"""Measure bird-vs-kite errors against independently annotated YOLO ground truth.

Usage:
 python tools/bird_kite_audit.py --model models/yolo26n_ncnn_model \
   --images validation/images --labels validation/labels \
   --cli /path/to/detectcore_cli --report bird_kite_report.json

Ground truth must be manually reviewed and include all visible birds/kites.
A screenshot with printed class captions is NOT a reliable training/evaluation set.
No thresholds or predictions are changed by this diagnostic tool.
"""
import argparse
import json
import math
import subprocess
from collections import Counter
from pathlib import Path

EXTENSIONS = {".jpg", ".jpeg", ".png", ".bmp", ".webp"}


def iou(a, b):
    left, top = max(a[0], b[0]), max(a[1], b[1])
    right, bottom = min(a[2], b[2]), min(a[3], b[3])
    intersection = max(0.0, right - left) * max(0.0, bottom - top)
    area_a = max(0.0, a[2] - a[0]) * max(0.0, a[3] - a[1])
    area_b = max(0.0, b[2] - b[0]) * max(0.0, b[3] - b[1])
    union = area_a + area_b - intersection
    return intersection / union if union > 0 else 0.0


def read_names(model):
    names = (model / "classes.txt").read_text(encoding="utf-8").splitlines()
    if names.count("bird") != 1 or names.count("kite") != 1:
        raise ValueError("Model must have exactly one 'bird' and one 'kite' class")
    return names.index("bird"), names.index("kite")


def load_annotations(path, width, height, valid_ids):
    if not path.is_file():
        raise FileNotFoundError(f"Missing human-reviewed ground truth: {path}")
    boxes = []
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip():
            continue
        parts = line.split()
        if len(parts) != 5:
            raise ValueError(f"{path}:{line_number}: expected YOLO 'class cx cy w h'")
        cls = int(parts[0])
        cx, cy, w, h = map(float, parts[1:])
        if cls not in valid_ids:
            continue
        if not all(map(math.isfinite, (cx, cy, w, h))) or not (
            0 <= cx <= 1 and 0 <= cy <= 1 and 0 < w <= 1 and 0 < h <= 1
        ) or cx - w / 2 < -1e-5 or cx + w / 2 > 1 + 1e-5 or cy - h / 2 < -1e-5 or cy + h / 2 > 1 + 1e-5:
            raise ValueError(f"{path}:{line_number}: invalid normalized bbox")
        boxes.append({"class_id": cls, "xyxy": [
            (cx - w / 2) * width, (cy - h / 2) * height,
            (cx + w / 2) * width, (cy + h / 2) * height
        ]})
    return boxes


def evaluate(model, images, labels, cli, confidence=0.25, nms=0.45, match_iou=0.5):
    # Pure standard-library image dimension parser for PNG/JPEG/BMP/WebP is brittle;
    # OpenCV is already a project prerequisite, use it only for ground-truth scaling.
    import cv2
    bird, kite = read_names(model)
    image_paths = sorted(p for p in images.rglob("*") if p.suffix.lower() in EXTENSIONS)
    if not image_paths:
        raise ValueError(f"No images under {images}")
    confusion = Counter()
    samples = []
    for image in image_paths:
        relative = image.relative_to(images)
        label = labels / relative.with_suffix(".txt")
        pixels = cv2.imread(str(image), cv2.IMREAD_COLOR)
        if pixels is None:
            raise ValueError(f"Cannot read image: {image}")
        height, width = pixels.shape[:2]
        gt = load_annotations(label, width, height, {bird, kite})
        cmd = [str(cli), str(model), str(image), str(confidence), str(nms), "--bird-kite-audit"]
        output = subprocess.run(cmd, capture_output=True, text=True, check=True)
        result = json.loads(output.stdout)
        det = [p for p in result["detections"] if p["class_id"] in (bird, kite)]
        evidence = result.get("bird_kite_evidence")
        if not isinstance(evidence, list) or len(evidence) != len(det):
            raise ValueError(f"CLI evidence missing or inconsistent for {image}")
        for p in det:
            if len(p["xyxy"]) != 4 or not all(map(math.isfinite, p["xyxy"])):
                raise ValueError(f"Bad detection coordinates for {image}")
        # Greedy class-AGNOSTIC IoU matching: a bird labelled kite must be counted
        # as a classification error, not a missed bird plus a false kite.
        edges = sorted(((iou(g["xyxy"], p["xyxy"]), gi, pi)
                        for gi, g in enumerate(gt) for pi, p in enumerate(det)), reverse=True)
        used_gt, used_det = set(), set()
        current = Counter()
        for overlap, gi, pi in edges:
            if overlap < match_iou:
                break
            if gi in used_gt or pi in used_det:
                continue
            used_gt.add(gi); used_det.add(pi)
            actual = "bird" if gt[gi]["class_id"] == bird else "kite"
            predicted = "bird" if det[pi]["class_id"] == bird else "kite"
            current[f"{actual}_as_{predicted}"] += 1
        for gi, g in enumerate(gt):
            if gi not in used_gt:
                current[("bird" if g["class_id"] == bird else "kite") + "_missed"] += 1
        for pi, p in enumerate(det):
            if pi not in used_det:
                current[("bird" if p["class_id"] == bird else "kite") + "_false_positive"] += 1
        confusion.update(current)
        samples.append({"image": str(relative).replace('\\', '/'), "ground_truth": len(gt),
                        "predicted": len(det), "counts": dict(current),
                        "evidence": evidence})
    return {"model": str(model), "images_evaluated": len(image_paths),
            "class_ids": {"bird": bird, "kite": kite},
            "confidence_threshold": confidence, "iou_threshold": nms,
            "match_iou": match_iou, "counts": dict(sorted(confusion.items())),
            "per_image": samples}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", type=Path, required=True)
    parser.add_argument("--images", type=Path, required=True)
    parser.add_argument("--labels", type=Path, required=True)
    parser.add_argument("--cli", type=Path, required=True)
    parser.add_argument("--confidence", type=float, default=0.25)
    parser.add_argument("--iou", type=float, default=0.45)
    parser.add_argument("--match-iou", type=float, default=0.5)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    if not (0 <= args.confidence <= 1 and 0 <= args.iou <= 1 and 0 < args.match_iou <= 1):
        parser.error("Threshold values out of range")
    report = evaluate(args.model, args.images, args.labels, args.cli,
                      args.confidence, args.iou, args.match_iou)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({k: v for k, v in report.items() if k != "per_image"}, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
