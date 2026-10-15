import hashlib,importlib.util,json,struct,unittest
from pathlib import Path

root=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('vein_hotkeys',root/'patches/vein-hotkeys.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
class HotkeyTests(unittest.TestCase):
 def setUp(self):
  self.manifest=json.loads((root/'patches/vein-hotkey.json').read_text())
  data=bytearray(self.manifest['inputImportOffset']+32)
  offset=self.manifest['inputImportOffset'];name=self.manifest['inputImportOriginal'].encode()+b'\0';data[offset:offset+len(name)]=name
  for site in self.manifest['sites']:
   code=bytes.fromhex(site['bytes']);data[site['offset']:site['offset']+len(code)]=code
  struct.pack_into('<I',data,11985,65536)
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
 def test_optional_modes_redirect_only_this_modules_input_and_restore(self):
  for mode in ('toggle','always'):
   changed=module.rekey(self.data,0x77,self.manifest,mode)
   offset=self.manifest['inputImportOffset']
   self.assertEqual(changed[offset:offset+11],b'vmkeys.dll\0')
   self.assertEqual(module.normalize(changed,self.manifest),(self.data,0x77))
   self.assertEqual(module.rekey(changed,192,self.manifest,'hold'),self.data)
  with self.assertRaises(ValueError):module.rekey(self.data,0x77,self.manifest,'unknown')
 def test_radius_survives_all_activation_modes_and_rebinding(self):
  expanded=bytearray(self.data);struct.pack_into('<I',expanded,11985,262144)
  for mode in ('hold','toggle','always'):
   changed=module.rekey(bytes(expanded),0x77,self.manifest,mode)
   self.assertEqual(struct.unpack_from('<I',changed,11985)[0],262144)
   self.assertEqual(changed[288536:288547],b'vmkeys.dll\0')
   self.assertEqual(module.normalize(changed,self.manifest),(self.data,0x77))
  with self.assertRaises(ValueError):module.rekey(bytes(expanded),0x7b,self.manifest)
 def test_rejects_arbitrary_region_capacity(self):
  changed=bytearray(self.data);struct.pack_into('<I',changed,11985,262145)
  with self.assertRaises(ValueError):module.rekey(changed,0x77,self.manifest)
