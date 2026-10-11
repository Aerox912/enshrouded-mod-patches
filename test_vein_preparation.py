import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('prepare_vein', ROOT / 'tools/prepare-vein-radius.py')
prepare_vein = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prepare_vein)


class PreparationTests(unittest.TestCase):
    def test_existing_destination_is_preserved_even_with_invalid_inputs(self):
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / 'existing'
            output.mkdir()
            sentinel = output / 'radius.ini'
            sentinel.write_bytes(b'[VeinMining]\nRadiusMultiplier=3\n')
            with self.assertRaisesRegex(ValueError, 'new staging directory'):
                prepare_vein.prepare('missing', 'missing', output, 'toggle')
            self.assertEqual(sentinel.read_bytes(), b'[VeinMining]\nRadiusMultiplier=3\n')

    def test_invalid_inputs_create_no_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for data in (b'wrong length', bytes(290816)):
                source = root / 'invalid.dll'
                source.write_bytes(data)
                with self.assertRaises(ValueError):
                    prepare_vein.prepare(source, source, root / 'output', 'hold')
                self.assertFalse((root / 'output').exists())


if __name__ == '__main__':
    unittest.main()
