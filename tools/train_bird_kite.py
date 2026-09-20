#!/usr/bin/env python3
"""Fine-tune YOLO26n on a human-labelled, representative COCO-80 dataset.

Example:
 python tools/train_bird_kite.py --weights models/yolo26n.pt \
    --data path/to/bird_kite_coco80.yaml --epochs 80 --project runs/bird_kite

IMPORTANT: keep all 80 original COCO class indices and include representative
existing classes in training; replacing the 80-class head with a two-class head
will break other ObjectDetectCore classes. Training requires ultralytics/torch
and real reviewed images. This script DOES NOT manufacture labels from screenshots.
"""
import argparse
from pathlib import Path


def validate_class_mapping(data_path):
    import yaml
    data = yaml.safe_load(data_path.read_text(encoding='utf-8'))
    if not isinstance(data, dict):
        raise ValueError('Dataset YAML must be a mapping')
    names = data.get('names')
    if isinstance(names, dict):
        ordered = [names.get(i, names.get(str(i))) for i in range(80)]
        if len(names) != 80:
            raise ValueError('Expected exactly 80 class names')
    elif isinstance(names, list):
        ordered = names
    else:
        raise ValueError('names: must be a list or dict')
    if len(ordered) != 80 or ordered[14] != 'bird' or ordered[33] != 'kite' or any(
        not isinstance(v, str) or not v for v in ordered
    ):
        raise ValueError('Keep the original 80 COCO class IDs (bird=14, kite=33)')
    if 'train' not in data or 'val' not in data or not data['train'] or not data['val']:
        raise ValueError('Dataset must have separate train: and val: splits')
    if data['train'] == data['val']:
        raise ValueError('Train and validation split cannot be identical')
    return data


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--weights', type=Path, required=True)
    ap.add_argument('--data', type=Path, required=True)
    ap.add_argument('--epochs', type=int, default=80)
    ap.add_argument('--batch', type=int, default=8)
    ap.add_argument('--imgsz', type=int, default=640)
    ap.add_argument('--project', type=Path, default=Path('runs/bird_kite'))
    ap.add_argument('--device', default='cpu')
    ap.add_argument('--workers', type=int, default=2)
    args = ap.parse_args()
    if not args.weights.is_file() or not args.data.is_file():
        ap.error('Missing weights or dataset YAML')
    if args.epochs < 1 or args.batch < 1 or args.workers < 0 or args.imgsz < 32 or args.imgsz % 32:
        ap.error('Invalid epochs, batch, workers or imgsz')
    validate_class_mapping(args.data)
    from ultralytics import YOLO
    model = YOLO(str(args.weights.resolve()))
    if model.task != 'detect':
        raise ValueError('Checkpoint must be YOLO object detection')
    if len(model.names) != 80 or model.names[14] != 'bird' or model.names[33] != 'kite':
        raise ValueError('Checkpoint class IDs disagree with COCO-80 ground truth')
    result = model.train(data=str(args.data.resolve()), epochs=args.epochs,
                         batch=args.batch, imgsz=args.imgsz, project=str(args.project),
                         name='finetune', device=args.device, workers=args.workers,
                         seed=42, deterministic=True, pretrained=True)
    print('Training finished. Review per-class precision/recall, confusion matrix,')
    print('all 80 COCO classes, and independent bird-vs-kite validation before deploying.')
    print('Next: export the resulting best.pt with tools/export_yolo26.py, run')
    print('tools/prepare_ncnn.py, and compare actual NCNN results on held-out images.')
    return result


if __name__ == '__main__':
    main()
