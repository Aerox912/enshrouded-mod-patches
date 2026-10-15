import base64
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import patcher

class PatchTests(unittest.TestCase):
    def test_patch_preserves_original(self):
        original=b'header\x00original\xfftail'; expected=b'header\x00replacement\xfftail'
        recipe={'kind':'delta','input':patcher.digest(original),'output':patcher.digest(expected),
                'edits':[{'offset':7,'remove':8,'data':base64.b64encode(b'replacement').decode()}]}
        self.assertEqual(patcher.transform(recipe,original),expected)
        with self.assertRaises(ValueError):patcher.transform(recipe,b'wrong version')
        recipe['output']='0'*64
        with self.assertRaises(ValueError):patcher.transform(recipe,original)
    def test_overlapping_patch_rejected(self):
        recipe={'kind':'delta','input':patcher.digest(b'abc'),'output':'0'*64,
                'edits':[{'offset':1,'remove':1,'data':''},{'offset':0,'remove':0,'data':''}]}
        with self.assertRaises(ValueError):patcher.transform(recipe,b'abc')
    def test_catalog_complete_and_scoped(self):
        catalog=json.loads((patcher.ROOT/'catalog.json').read_text())
        components={c['id']:c for c in catalog['components']}
        self.assertEqual(len(components),20)
        self.assertEqual(components['flight']['targets'],['client'])
        self.assertTrue(components['workshop']['optional'])
        self.assertEqual(components['workshop']['targets'],['client','server'])
        self.assertEqual(components['global-xp']['targets'],['server'])
        self.assertEqual(components['global-xp']['distribution'],'original-import')
        for profile in ['normal','cheeze']:
            data=json.loads((patcher.ROOT/f'profiles/{profile}.json').read_text())
            self.assertIn('global-xp',data['server']);self.assertNotIn('global-xp',data['client'])
            self.assertEqual(len(data['emberFeatures']),5)
            self.assertTrue(data['emberBuildInFog']);self.assertTrue(data['emberAltarRequired'])
            for target in ['server','client']:
                for id in data[target]:
                    self.assertIn(target,components[id]['targets']);self.assertIn(profile,components[id]['profiles'])
        normal=json.loads((patcher.ROOT/'profiles/normal.json').read_text())
        self.assertNotIn('vein',normal['server'])
    def test_original_files_have_verified_metadata(self):
        catalog=json.loads((patcher.ROOT/'catalog.json').read_text())
        for component in catalog['components']:
            if component['distribution']!='original-import':continue
            self.assertTrue(component['files'])
            for file in component['files']:
                self.assertRegex(file['sha256'],r'^[0-9a-f]{64}$');self.assertGreater(file['size'],0)
                self.assertNotIn('..',file['path'].split('/'))
    def test_recipe_identity_and_no_replacement_files(self):
        for recipe in json.loads((patcher.ROOT/'patches/recipes.json').read_text()).values():
            self.assertRegex(recipe['input'],r'^[0-9a-f]{64}$')
            self.assertRegex(recipe['output'],r'^[0-9a-f]{64}$')
            self.assertNotEqual(recipe['input'],recipe['output'])
    def test_auto_loot_client_import_uses_matching_verified_patch(self):
        catalog=json.loads((patcher.ROOT/'catalog.json').read_text())
        component=next(c for c in catalog['components'] if c['id']=='auto-loot')
        original=next(f for f in component['files'] if f['path']=='src/mod.lua')
        imports=json.loads((patcher.ROOT/'client-imports.json').read_text())
        entry=next(i for i in imports if i['component']=='auto-loot' and i['source']=='src/mod.lua')
        recipe=json.loads((patcher.ROOT/'patches/recipes.json').read_text())[entry['patch']]
        files=json.loads((patcher.ROOT/'client-files.json').read_text())['Files']
        target=next(f for f in files if f['Path']==entry['target'])
        self.assertEqual(entry['patch'],component['maintainedPatch']['recipe'])
        self.assertEqual(target['Patch'],entry['patch'])
        self.assertEqual(recipe['input'],original['sha256'])
        self.assertEqual(entry['inputHash'],recipe['input'])
        self.assertEqual(target['OriginalHash'],recipe['input'])
        self.assertEqual(target['Hash'],recipe['output'])
        self.assertEqual(recipe['input'],'a117a4ec25de51479c299f97ff1b231b9dba9541b6521317519add8844ded773')
        self.assertEqual(recipe['output'],'e171920edc367d5fa999740e7128f9461723aa8c8c2f1c83e97fb9730232b303')
        delta=sum(len(base64.b64decode(e['data']))-e['remove'] for e in recipe['edits'])
        self.assertEqual(target['Size'],original['size']+delta)
        patch_text=''.join(base64.b64decode(e['data']).decode() for e in recipe['edits'])
        self.assertIn('beginsWith(name, "LootPickup_Material_Critter_Parts_")',patch_text)
        self.assertIn('name == "LootPickup_Material_Skeleton_Weakling_Bones"',patch_text)
        self.assertIn('name == "LootPickup_Material_Skeleton_Hound_Bones"',patch_text)

    def test_auto_loot_server_assembly_uses_the_same_verified_patch(self):
        recipe=json.loads((patcher.ROOT/'patches/recipes.json').read_text())['auto-loot-critters']
        transformed=[]
        def find_original(_directory,spec):
            return b'original-auto-loot' if spec['sha256']==recipe['input'] else b'original'
        def transform(spec,data):
            transformed.append((spec,data))
            return b'patched:'+spec['output'].encode()
        with tempfile.TemporaryDirectory(dir=patcher.ROOT) as directory:
            root=Path(directory)
            adapter=root/'adapter.dll';adapter.write_bytes(b'adapter')
            destination=root/'prepared'
            with patch.object(patcher,'find_original',side_effect=find_original):
                with patch.object(patcher,'transform',side_effect=transform):
                    patcher.server_profile('cheeze',root,destination,adapter=adapter)
            assembled=(destination/'mods/XHL-Auto-Loot/src/mod.lua').read_bytes()
        self.assertIn((recipe,b'original-auto-loot'),transformed)
        self.assertEqual(assembled,b'patched:'+recipe['output'].encode())

if __name__=='__main__':unittest.main()
