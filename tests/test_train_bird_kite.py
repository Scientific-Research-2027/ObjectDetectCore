"""Configuration guard tests only; no model is trained in this environment."""
import sys
import tempfile
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from train_bird_kite import validate_class_mapping


class ModelMappingTests(unittest.TestCase):
    def test_correct_coco_mapping_accepted(self):
        with tempfile.TemporaryDirectory() as folder:
            p=Path(folder)/'data.yaml'
            names=[f'class_{i}' for i in range(80)]
            names[14]='bird';names[33]='kite'
            import yaml
            p.write_text(yaml.safe_dump({'names': names, 'train': 'train/images', 'val': 'val/images'}), encoding='utf-8')
            self.assertEqual(validate_class_mapping(p)['names'][33], 'kite')
            names[33]='bird'
            p.write_text(yaml.safe_dump({'names': names, 'train': 'train/images', 'val': 'val/images'}), encoding='utf-8')
            with self.assertRaises(ValueError): validate_class_mapping(p)

    def test_reject_same_train_val(self):
        with tempfile.TemporaryDirectory() as folder:
            p=Path(folder)/'data.yaml'
            names=[f'class_{i}' for i in range(80)]
            names[14]='bird';names[33]='kite'
            import yaml
            p.write_text(yaml.safe_dump({'names': names, 'train': 'same', 'val': 'same'}), encoding='utf-8')
            with self.assertRaises(ValueError): validate_class_mapping(p)


if __name__=='__main__': unittest.main()
