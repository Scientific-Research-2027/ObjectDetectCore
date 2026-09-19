"""Compare C++ CLI detections with Ultralytics NCNN backend on one identical image.
Usage: python tools/compare_reference.py MODEL_DIR IMAGE_PATH /path/to/detectcore_cli
Requires ultralytics, opencv-python, numpy; calls actual backend, does not invent metrics.
"""
import argparse
import json
import subprocess
import sys
from pathlib import Path
import numpy as np
import cv2
from ultralytics import YOLO

def iou(a, b):
    x1 = max(a[0], b[0]); y1 = max(a[1], b[1]); x2 = min(a[2], b[2]); y2 = min(a[3], b[3])
    intersection = max(0, x2-x1)*max(0, y2-y1)
    area_a = max(0, a[2]-a[0])*max(0,a[3]-a[1])
    area_b = max(0, b[2]-b[0])*max(0,b[3]-b[1])
    return intersection/(area_a+area_b-intersection) if area_a+area_b>intersection else 0.0

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("model",type=Path);ap.add_argument("image",type=Path)
    ap.add_argument("cli",type=Path)
    ap.add_argument("--confidence",type=float,default=0.25)
    ap.add_argument("--iou",type=float,default=0.45)
    args=ap.parse_args()
    image_bytes = np.fromfile(str(args.image), dtype=np.uint8)
    image=cv2.imdecode(image_bytes,cv2.IMREAD_COLOR)
    if image is None:raise ValueError("Cannot read image")
    cfg={}
    for line in (args.model/"detectcore.cfg").read_text(encoding="utf-8").splitlines():
        if "=" in line: k,v=line.split("=",1);cfg[k]=v
    if int(cfg["width"])!=int(cfg["height"]):
        raise ValueError("This reference comparator currently supports square export imgsz only")
    # Use the same NCNN weights/export, image size, thresholds and letterbox; compare detection outputs.
    reference=YOLO(str(args.model)).predict(image, imgsz=int(cfg["width"]),
        conf=args.confidence, iou=args.iou, verbose=False, device="cpu")[0]
    cmd=[str(args.cli.resolve()),str(args.model.resolve()),str(args.image.resolve()),
         str(args.confidence),str(args.iou)]
    ours=json.loads(subprocess.check_output(cmd,text=True))
    ref=[{"class_id":int(c),"score":float(s),"xyxy":b.tolist()} for b,s,c in zip(
        reference.boxes.xyxy.cpu().numpy(),reference.boxes.conf.cpu().numpy(),reference.boxes.cls.cpu().numpy())]
    unmatched=set(range(len(ref)));matched=[]
    for own in ours["detections"]:
        eligible=[(iou(own["xyxy"],ref[j]["xyxy"]),j) for j in unmatched
                  if own["class_id"]==ref[j]["class_id"]]
        if not eligible:continue
        score,j=max(eligible)
        if score<0.5:continue
        unmatched.remove(j)
        matched.append((score,abs(own["score"]-ref[j]["score"]),
                        max(abs(a-b) for a,b in zip(own["xyxy"],ref[j]["xyxy"]))))
    result={"cpp_count":len(ours["detections"]),"reference_count":len(ref),
            "matched_iou_ge_0_5":len(matched),"unmatched_reference":len(unmatched),
            "unmatched_cpp":len(ours["detections"])-len(matched),
            "min_iou":min((v[0] for v in matched),default=None),
            "max_abs_score_delta":max((v[1] for v in matched),default=None),
            "max_abs_bbox_delta_pixels":max((v[2] for v in matched),default=None),
            "cpp_timing_ms":ours["timing_ms"]}
    print(json.dumps(result,ensure_ascii=False,indent=2))
    if len(matched)!=len(ref) or len(matched)!=len(ours["detections"]):
        sys.exit("REFERENCE MISMATCH: inspect preprocessing/output layout and scores")
if __name__=="__main__":main()
