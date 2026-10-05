"""Record release provenance and profile-specific dependency fingerprints."""
import ast,hashlib,json,subprocess
from pathlib import Path

def fingerprint(target, root=None):
 root=Path(root) if root is not None else Path(__file__).resolve().parents[1]
 catalog=json.loads((root/'catalog.json').read_text(encoding='utf-8-sig'))
 recipes=json.loads((root/'patches/recipes.json').read_text(encoding='utf-8-sig'))
 selected=[c for c in catalog['components'] if target in c['targets']]
 profiles=[json.loads((root/f'profiles/{p}.json').read_text(encoding='utf-8-sig')) for p in ['normal','cheeze']]
 keys=[target,'emberFeatures','emberBuildInFog','emberAltarRequired']+(['serverSettings','serverDllOverrides'] if target=='server' else [])
 data={'game':{k:v for k,v in catalog['game'].items() if k.startswith(target)},'components':selected,'profiles':[{k:p[k] for k in keys} for p in profiles]}
 imports=json.loads((root/'client-imports.json').read_text(encoding='utf-8-sig'))
 names={i['patch'] for i in imports if i['patch']} if target=='client' else {'rested-server'}
 data['recipes']={name:recipes[name] for name in sorted(names)}
 data['ember']=(root/'defaults/Ember.lua').read_text(encoding='utf-8-sig')
 functions={'digest','transform','audio48k','patch'} if target=='client' else {'digest','transform','server_profile','find_original','safe'}
 tree=ast.parse((root/'patcher.py').read_text(encoding='utf-8-sig'))
 data['implementation']=[ast.dump(node,include_attributes=False) for node in tree.body if isinstance(node,(ast.Import,ast.ImportFrom)) or isinstance(node,(ast.FunctionDef,ast.AsyncFunctionDef)) and node.name in functions]
 data['build']=(root/'ci/build.ps1').read_text(encoding='utf-8-sig')
 if target=='client':
  data['files']=json.loads((root/'client-files.json').read_text(encoding='utf-8-sig'));data['imports']=imports
  data['vein']=(root/'patches/localize-vein-mining.py').read_text(encoding='utf-8-sig');data['translation']=json.loads((root/'patches/vein-english.json').read_text(encoding='utf-8-sig'))
 else:data['xpSettings']=(root/'defaults/safeprobe_config.ini').read_text(encoding='utf-8-sig')
 return hashlib.sha256(json.dumps(data,sort_keys=True,separators=(',',':')).encode()).hexdigest()

if __name__=='__main__':
 root=Path(__file__).resolve().parents[1];catalog=json.loads((root/'catalog.json').read_text(encoding='utf-8-sig'))
 info={'version':catalog['version'],'commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),'repository':'Aerox912/enshrouded-mod-patches','clientFingerprint':fingerprint('client'),'serverFingerprint':fingerprint('server')}
 (root/'build/package/build-info.json').write_text(json.dumps(info,indent=2)+'\n')
