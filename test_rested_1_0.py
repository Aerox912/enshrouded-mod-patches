"""Public hash-pinned Rested patch contract; native behavior needs gameplay tests."""
import json
import unittest
import patcher


class RestedCompatibilityTests(unittest.TestCase):
    def test_rested_patch_requires_the_known_hoarder_compatible_original(self):
        recipes = json.loads((patcher.ROOT / 'patches/recipes.json').read_text())
        self.assertIn('rested-1.0', recipes)
        recipe = recipes['rested-1.0']
        self.assertEqual(recipe['input'], '3123d2d0bb9c38a39a30adc1b9979306c009576cce1a04654cf38be31619e29e')
        with self.assertRaisesRegex(ValueError, 'checksum'):
            patcher.transform(recipe, b'unsupported bytecode source')


if __name__ == '__main__':
    unittest.main()
