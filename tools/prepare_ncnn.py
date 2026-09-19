"""Derive NCNN blob names and class names from an actual exported model; no guessed names.
Usage: python tools/prepare_ncnn.py path/to/yolo26n_ncnn_model
Requires: PyYAML (installed alongside Ultralytics). No model downloading.
"""
import argparse
import json
from pathlib import Path
import yaml

def read_graph(param):
    rows = param.read_text(encoding="utf-8-sig").splitlines()
    if len(rows) < 3 or rows[0].strip() != "7767517":
        raise ValueError("NCNN textual .param magic invalid; did you supply model.ncnn.param?")
    layers = []
    for n, line in enumerate(rows[2:], start=3):
        line = line.strip()
        if not line:
            continue
        parts = line.split()
        if len(parts) < 4:
            raise ValueError(f"Line {n}: truncated NCNN layer")
        bottom_count, top_count = int(parts[2]), int(parts[3])
        fields = parts[4:]
        if len(fields) < bottom_count + top_count:
            raise ValueError(f"Line {n}: missing bottom/top blobs")
        bottoms = fields[:bottom_count]
        tops = fields[bottom_count:bottom_count + top_count]
        layers.append((parts[0], bottoms, tops, fields[bottom_count + top_count:]))
    inputs = [top for kind, _, tops, _ in layers if kind in ("Input", "pnnx.Input") for top in tops]
    consumers = {b for _, bottoms, _, _ in layers for b in bottoms}
    finals = [top for _, _, tops, _ in layers for top in tops if top not in consumers]
    # Some ncnn graphs use an explicit pnnx.Output with no top: the bottom is the output.
    finals += [b for kind, bottoms, tops, _ in layers if kind in ("pnnx.Output", "Output")
               and not tops for b in bottoms]
    finals = list(dict.fromkeys(finals))
    if len(inputs) != 1 or len(finals) != 1:
        raise ValueError(f"Detection MVP expects one input/output: inputs={inputs} outputs={finals}")
    return inputs[0], finals[0]

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("directory", type=Path)
    args = ap.parse_args()
    p = args.directory.resolve()
    for name in ("model.ncnn.param", "model.ncnn.bin", "metadata.yaml"):
        if not (p / name).is_file():
            raise FileNotFoundError(p / name)
    meta = yaml.safe_load((p / "metadata.yaml").read_text(encoding="utf-8"))
    if not isinstance(meta, dict) or meta.get("task") != "detect":
        raise ValueError("Only single-output YOLO26 object detection is supported")
    raw_names = meta.get("names")
    if isinstance(raw_names, dict):
        names_by_id = {int(k): str(v) for k, v in raw_names.items()}
        if sorted(names_by_id) != list(range(len(names_by_id))):
            raise ValueError("Non-contiguous class indices in metadata")
        names = [names_by_id[i] for i in range(len(names_by_id))]
    elif isinstance(raw_names, list):
        names = list(map(str, raw_names))
    else:
        raise ValueError("metadata.yaml must contain names: mapping or list")
    if not names or any("\n" in n or not n for n in names):
        raise ValueError("Invalid class names")
    imgsz = meta.get("imgsz")
    if isinstance(imgsz, int):
        height = width = imgsz
    elif isinstance(imgsz, (tuple, list)) and len(imgsz) == 2:
        height, width = map(int, imgsz)
    else:
        raise ValueError(f"Missing/unsupported metadata imgsz: {imgsz!r}")
    if width <= 0 or height <= 0:
        raise ValueError("Invalid input size")
    input_name, output_name = read_graph(p / "model.ncnn.param")
    (p / "detectcore.cfg").write_text(
        f"input={input_name}\noutput={output_name}\nwidth={width}\nheight={height}\n"
        "layout=raw_xywh\n", encoding="utf-8")
    (p / "classes.txt").write_text("\n".join(names) + "\n", encoding="utf-8")
    print(json.dumps({"input": input_name, "output": output_name, "width": width,
                      "height": height, "classes": len(names), "layout": "raw_xywh"},
                     ensure_ascii=False, indent=2))
    print("CAUTION: layout=raw_xywh is a required export contract. First C++ inference validates shape; "
          "reference parity must still be tested with the actual exported model.")

if __name__ == "__main__":
    main()
