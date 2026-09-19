"""Export true YOLO26n NCNN raw one-to-many outputs; require a local .pt checkpoint.
Usage: python tools/export_yolo26.py path/to/yolo26n.pt --imgsz 640
"""
import argparse
import json
import platform
from pathlib import Path
from importlib.metadata import version
from ultralytics import YOLO

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("weights",type=Path)
    parser.add_argument("--imgsz",type=int,default=640)
    args=parser.parse_args()
    if not args.weights.is_file():raise FileNotFoundError(args.weights)
    if args.imgsz<32 or args.imgsz%32:raise ValueError("imgsz must be positive and divisible by 32")
    model=YOLO(str(args.weights.resolve()))
    if model.task!="detect":raise ValueError("Only detection checkpoints are supported")
    # For NCNN, Ultralytics falls back to RAW one-to-many for unsupported end-to-end graphs.
    output=Path(model.export(format="ncnn",imgsz=args.imgsz,batch=1,device="cpu"))
    record={"source_weights":str(args.weights.resolve()),"format":"ncnn",
      "imgsz":args.imgsz,"batch":1,"device":"cpu","nms":None,
      "ultralytics":version("ultralytics"),"ncnn":version("ncnn"),
      "pnnx":version("pnnx"),"python":platform.python_version(),"platform":platform.platform()}
    (output/"detectcore-export.json").write_text(json.dumps(record,indent=2),encoding="utf-8")
    print(f"Export complete: {output}")
    print(f"Next: python tools/prepare_ncnn.py {output}")
if __name__=="__main__":main()
