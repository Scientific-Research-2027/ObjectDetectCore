"""Synthetic audit test; not a test of the provided NCNN checkpoint."""
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import bird_kite_audit as audit


class FakeResponse:
    def __init__(self, payload):
        self.stdout = json.dumps(payload)


class AuditTests(unittest.TestCase):
    def test_class_agnostic_iou_records_misclassification(self):
        import cv2
        import numpy as np
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            model = root / 'model'; model.mkdir()
            (model / 'classes.txt').write_text('\n'.join('bird' if k == 14 else 'kite' if k == 33 else f'class_{k}' for k in range(80)), encoding='utf-8')
            images = root / 'images'; labels = root / 'labels'
            images.mkdir(); labels.mkdir()
            cv2.imwrite(str(images / 'bird.png'), np.zeros((100, 100, 3), dtype=np.uint8))
            cv2.imwrite(str(images / 'kite.png'), np.zeros((100, 100, 3), dtype=np.uint8))
            (labels / 'bird.txt').write_text('14 0.5 0.5 0.4 0.4\n')
            (labels / 'kite.txt').write_text('33 0.5 0.5 0.4 0.4\n')
            prediction = {'detections': [{'class_id': 14, 'score': 0.9, 'xyxy': [30, 30, 70, 70]}],
                          'bird_kite_evidence': [{'class_id': 14, 'candidate_index': 1, 'bird_score': 0.9, 'kite_score': 0.8, 'xyxy': [30, 30, 70, 70]}]}
            with patch.object(audit.subprocess, 'run', return_value=FakeResponse(prediction)) as mock_run:
                report = audit.evaluate(model, images, labels, Path('fake_cli'))
            self.assertEqual(mock_run.call_count, 2)
            self.assertEqual(report['counts'], {'bird_as_bird': 1, 'kite_as_bird': 1})
            self.assertEqual(report['images_evaluated'], 2)

    def test_annotation_validation(self):
        with tempfile.TemporaryDirectory() as folder:
            p = Path(folder) / 'label.txt'
            p.write_text('33 0.5 0.5 1.2 0.3\n')
            with self.assertRaises(ValueError):
                audit.load_annotations(p, 100, 100, {14, 33})
            p.unlink()
            with self.assertRaises(FileNotFoundError):
                audit.load_annotations(p, 100, 100, {14, 33})


if __name__ == '__main__':
    unittest.main()
