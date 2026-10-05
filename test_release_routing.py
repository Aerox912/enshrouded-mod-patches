import importlib.util,json,shutil,tempfile,unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('release_info',ROOT/'tools/release-info.py')
release_info=importlib.util.module_from_spec(spec);spec.loader.exec_module(release_info)

class ReleaseRouting(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)
  for name in ['catalog.json','client-files.json','client-imports.json','patcher.py']:
   shutil.copy2(ROOT/name,self.root/name)
  for name in ['profiles','patches','defaults','ci','native']:
   shutil.copytree(ROOT/name,self.root/name)
  self.client=release_info.fingerprint('client',self.root)
  self.server=release_info.fingerprint('server',self.root)
 def tearDown(self):self.temp.cleanup()
 def mutate_json(self,name,change):
  path=self.root/name;data=json.loads(path.read_text());change(data);path.write_text(json.dumps(data))
 def test_server_config_does_not_trigger_client(self):
  with (self.root/'defaults/safeprobe_config.ini').open('a') as f:f.write('\n; server configuration revision\n')
  self.assertEqual(self.client,release_info.fingerprint('client',self.root))
  self.assertNotEqual(self.server,release_info.fingerprint('server',self.root))
 def test_server_patch_does_not_trigger_client(self):
  self.mutate_json('patches/recipes.json',lambda d:d['rested-server'].update(output_sha256='f'*64))
  self.assertEqual(self.client,release_info.fingerprint('client',self.root))
  self.assertNotEqual(self.server,release_info.fingerprint('server',self.root))
 def test_server_adapter_does_not_trigger_client(self):
  self.mutate_json('catalog.json',lambda d:next(c for c in d['components'] if c['id']=='server-adapter').update(version='next'))
  self.assertEqual(self.client,release_info.fingerprint('client',self.root))
  self.assertNotEqual(self.server,release_info.fingerprint('server',self.root))
 def test_client_release_does_not_trigger_server(self):
  self.mutate_json('catalog.json',lambda d:next(c for c in d['components'] if c['id']=='minimap').update(version='next'))
  self.assertNotEqual(self.client,release_info.fingerprint('client',self.root))
  self.assertEqual(self.server,release_info.fingerprint('server',self.root))
 def test_shared_ember_defaults_trigger_both(self):
  with (self.root/'defaults/Ember.lua').open('a') as f:f.write('\n-- Shared profile revision\n')
  self.assertNotEqual(self.client,release_info.fingerprint('client',self.root))
  self.assertNotEqual(self.server,release_info.fingerprint('server',self.root))
 def test_catalog_version_alone_triggers_neither(self):
  self.mutate_json('catalog.json',lambda d:d.update(version='999.0.0'))
  self.assertEqual(self.client,release_info.fingerprint('client',self.root))
  self.assertEqual(self.server,release_info.fingerprint('server',self.root))
 def test_controller_input_changes_trigger_clients_only(self):
  with (self.root/'native/vein-controls/activation.h').open('a') as f:f.write('\n// New client input behavior\n')
  self.assertNotEqual(self.client,release_info.fingerprint('client',self.root))
  self.assertEqual(self.server,release_info.fingerprint('server',self.root))

if __name__=='__main__':unittest.main()
