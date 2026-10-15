"""Patch contracts; original-mod execution is separately tested on copied game data."""
import json
import os
from pathlib import Path
import unittest

import patcher

RECIPES = {
    'hoarder-1.0': ('HoardersHelper', '2b5f3b087508a484dd907de457446903873171106bd5ac06061e04a282dd6253'),
    'blink-1.0': ('ElectricBlink', 'e91de259f8fbfa616f7ee63f06556b4ba96738a2249d7791e9a80c2660de6a94'),
    'creative-forges-1.0': ('creative-gem-forges', '01e083bc3b07acec714df0185424a65285b13897610908d9de725e1a7bcde4b0'),
}


class CompatibilityTests(unittest.TestCase):
    def setUp(self):
        self.recipes = json.loads((patcher.ROOT / 'patches/recipes.json').read_text())

    def test_three_hash_pinned_compatibility_patches_exist(self):
        self.assertTrue(RECIPES.keys() <= self.recipes.keys())
        for name, (_, digest) in RECIPES.items():
            self.assertEqual(self.recipes[name]['input'], digest)
            self.assertNotEqual(self.recipes[name]['output'], digest)

    def test_modified_or_wrong_original_is_rejected(self):
        self.assertTrue(RECIPES.keys() <= self.recipes.keys())
        for name in RECIPES:
            with self.assertRaisesRegex(ValueError, 'checksum'):
                patcher.transform(self.recipes[name], b'another author version')

    @unittest.skipUnless(os.environ.get('ESM_ORIGINAL_MODS'), 'requires privately supplied original mods')
    def test_private_originals_transform_without_losing_guards(self):
        root = Path(os.environ['ESM_ORIGINAL_MODS'])
        self.assertTrue(RECIPES.keys() <= self.recipes.keys())
        output = {}
        for name, (folder, _) in RECIPES.items():
            source = (root / folder / 'src/mod.lua').read_bytes()
            result = patcher.transform(self.recipes[name], source)
            self.assertEqual(patcher.digest(result), self.recipes[name]['output'])
            output[name] = result.decode('utf-8-sig')
        self.assertNotIn('loc.dataHash', output['hoarder-1.0'])
        self.assertIn('loc.data.hash = game.guid.to_content_hash(new_content.guid)', output['hoarder-1.0'])
        self.assertIn('streamedData = make_streamed_reference(data_hash)', output['blink-1.0'])
        self.assertIn('category = { value = reference.category.value }', output['blink-1.0'])
        self.assertIn('sound_mode == "none"', output['blink-1.0'])
        self.assertIn('"keen::ecs::InteractionOfferRuntime"', output['creative-forges-1.0'])
        self.assertIn('defaultSequence.value)~=4201246690', output['creative-forges-1.0'])
        self.assertIn('full Lua-visible target template snapshot changed', output['creative-forges-1.0'])


if __name__ == '__main__':
    unittest.main()
