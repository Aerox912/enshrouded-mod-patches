import hashlib,importlib.util,json,struct,unittest
from pathlib import Path

root=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('vein_hotkeys',root/'patches/vein-hotkeys.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
class HotkeyTests(unittest.TestCase):
 def setUp(self):
  self.manifest=json.loads((root/'patches/vein-hotkey.json').read_text())
  data=bytearray(30000)
  for site in self.manifest['sites']:
   code=bytes.fromhex(site['bytes']);data[site['offset']:site['offset']+len(code)]=code
  self.data=bytes(data);self.manifest['hashes']=[hashlib.sha256(self.data).hexdigest()]
 def test_changes_only_key_operands_and_restores(self):
  changed=module.rekey(self.data,0x76,self.manifest)
  self.assertEqual({i for i,(a,b) in enumerate(zip(self.data,changed)) if a!=b},{s['offset']+1 for s in self.manifest['sites']})
  self.assertEqual(module.normalize(changed,self.manifest),(self.data,0x76))
  self.assertEqual(module.rekey(changed,192,self.manifest),self.data)
 def test_rejects_wrong_versions_and_instruction_or_key_tampering(self):
  for index in [200,self.manifest['sites'][0]['offset'],self.manifest['sites'][1]['offset']+1]:
   altered=bytearray(self.data);altered[index]^=1
   with self.assertRaises(ValueError):module.rekey(altered,0x76,self.manifest)
 def test_invalid_keys_and_truncation(self):
  for key in [0,255,-1,256,'Home']:
   with self.assertRaises(ValueError):module.rekey(self.data,key,self.manifest)
  with self.assertRaises(ValueError):module.rekey(self.data[:100],0x76,self.manifest)
