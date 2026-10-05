import contextlib,io,json,tempfile,unittest,zipfile
from pathlib import Path
from unittest.mock import patch
import patcher

class ServerProfiles(unittest.TestCase):
 def test_both_profiles_import_xp_without_rebuilding_it(self):
  real=patcher.ROOT
  with tempfile.TemporaryDirectory() as temp:
   root=Path(temp);originals=root/'originals';originals.mkdir();config=root/'defaults';config.mkdir();profiles=root/'profiles';profiles.mkdir()
   original=b'unchanged upstream XP fixture';adapter=b'authored adapter fixture'
   with zipfile.ZipFile(originals/'upstream.zip','w') as z:z.writestr('author/dbghelp.dll',original)
   (root/'adapter.dll').write_bytes(adapter)
   (root/'catalog.json').write_text(json.dumps({'components':[{'id':'global-xp','distribution':'original-import','files':[{'path':'dbghelp.dll','size':len(original),'sha256':patcher.digest(original)}]}]}))
   for name in ['Ember.lua','safeprobe_config.ini']:(config/name).write_bytes((real/'defaults'/name).read_bytes())
   for profile in ['normal','cheeze']:(profiles/f'{profile}.json').write_text(json.dumps({'server':['global-xp']}))
   with patch.object(patcher,'ROOT',root),contextlib.redirect_stdout(io.StringIO()):
    patcher.server_profile('normal',originals,root/'normal')
    patcher.server_profile('cheeze',originals,root/'cheeze',root/'adapter.dll')
    self.assertEqual((root/'normal/dbghelp.dll').read_bytes(),original)
    self.assertFalse((root/'normal/GlobalXPShare.original.dll').exists())
    self.assertEqual((root/'cheeze/GlobalXPShare.original.dll').read_bytes(),original)
    self.assertEqual((root/'cheeze/dbghelp.dll').read_bytes(),adapter)
    self.assertEqual((root/'normal/mods/Ember/src/User_Config_Overrides.lua').read_bytes(),(root/'cheeze/mods/Ember/src/User_Config_Overrides.lua').read_bytes())
    self.assertIn('ShareMultiplier=1.0',(root/'normal/safeprobe_config.ini').read_text())
    with self.assertRaises(ValueError):patcher.server_profile('normal',originals,root/'normal')
    (originals/'upstream.zip').unlink()
    with self.assertRaises(ValueError):patcher.server_profile('normal',originals,root/'missing')
    self.assertFalse((root/'missing').exists())
 def test_unsafe_package_paths_rejected(self):
  with tempfile.TemporaryDirectory() as temp:
   for path in ['../escape','C:/escape','/escape','mods/../../escape','mods\\escape']:
    with self.assertRaises(ValueError):patcher.safe(Path(temp),path)
 def test_profiles_preserve_durability_and_altar_requirements(self):
  root=patcher.ROOT
  normal=json.loads((root/'profiles/normal.json').read_text())
  cheeze=json.loads((root/'profiles/cheeze.json').read_text())
  self.assertEqual(normal['serverSettings'],{})
  self.assertFalse(cheeze['serverSettings']['gameSettings']['enableDurability'])
  defaults=(root/'defaults/Ember.lua').read_text()
  for option in normal['emberFeatures']:self.assertIn(option+' = true',defaults)
  self.assertIn('PlacementTweaks_BuildInFog = true',defaults)
  self.assertIn('PlacementTweaks_NoBuildZoneNeeded = false',defaults)
  self.assertNotIn('area-harvest',normal['client']+normal['server']+cheeze['client']+cheeze['server'])
if __name__=='__main__':unittest.main()
