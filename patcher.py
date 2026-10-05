"""Verified local transformations. Original mods are never bundled or overwritten."""
import argparse
import base64
import hashlib
import importlib.util
import json
import re
import sys
import zipfile
from pathlib import Path

ROOT = Path(getattr(sys, '_MEIPASS', Path(__file__).resolve().parent))

def digest(data):
    return hashlib.sha256(data).hexdigest()

def audio48k(data):
    # Python 3.12 is pinned for audioop's deterministic signed PCM resampler.
    import audioop
    text = data.decode('utf-8-sig')
    if int(re.search(r'sampleRate\s*=\s*(\d+)', text)[1]) != 44100:
        raise ValueError('Expected 44.1 kHz source audio')
    raw = bytes.fromhex(''.join(re.findall(r'"([0-9a-fA-F]+)"', text)))
    count = int(re.search(r'frameCount\s*=\s*(\d+)', text)[1])
    if len(raw) != count * 2:
        raise ValueError('Invalid original PCM length')
    result, _ = audioop.ratecv(raw, 2, 1, 44100, 48000, None)
    frames = len(result) // 2
    encoded = result.hex()
    chunks = [encoded[i:i+2000] for i in range(0, len(encoded), 2000)]
    lines = ['return {', '  sampleRate = 48000,', '  channels = 1, -- Mono',
             '  format = "Uncompressed",', f'  frameCount = {frames},',
             f'  durationNs = {frames * 1000000000 // 48000},',
             '  -- hex string of the raw PCM bytes (s16le), unbroken or chunked and concatenated',
             '  hex = "' + chunks[0] + '"']
    lines += ['    .. "' + chunk + '"' for chunk in chunks[1:]]
    lines += ['}', '']
    return '\r\n'.join(lines).encode('utf-8')

def transform(recipe, data):
    if digest(data) != recipe['input']:
        raise ValueError('Original file checksum does not match the supported version')
    if recipe['kind'] == 'audio48k':
        result = audio48k(data)
    elif recipe['kind'] == 'delta':
        pieces = []; cursor = 0
        for edit in recipe['edits']:
            offset, remove = edit['offset'], edit['remove']
            if not isinstance(offset, int) or not isinstance(remove, int) or offset < cursor or remove < 0 or offset + remove > len(data):
                raise ValueError('Invalid patch range')
            pieces.extend((data[cursor:offset], base64.b64decode(edit['data'], validate=True)))
            cursor = offset + remove
        pieces.append(data[cursor:]); result = b''.join(pieces)
    else:
        raise ValueError('Unsupported in-memory transformation')
    if digest(result) != recipe['output']:
        raise ValueError('Patched file checksum mismatch')
    return result

def patch(name, source, destination):
    recipes = json.loads((ROOT/'patches/recipes.json').read_text(encoding='utf-8'))
    recipe = recipes[name]
    source, destination = Path(source), Path(destination)
    if source.resolve() == destination.resolve() or destination.exists():
        raise ValueError('Use a separate, new output file; preserve the original')
    data = source.read_bytes()
    if digest(data) != recipe['input']:
        raise ValueError('Unsupported original version')
    if recipe['kind'] == 'vein':
        spec = importlib.util.spec_from_file_location('vein_patch', ROOT/'patches/localize-vein-mining.py')
        module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
        module.localize(source, destination, ROOT/'patches/vein-english.json')
        if digest(destination.read_bytes()) != recipe['output']:
            destination.unlink(); raise ValueError('Vein patch output mismatch')
    else:
        result = transform(recipe, data)
        destination.parent.mkdir(parents=True, exist_ok=True)
        with destination.open('xb') as output: output.write(result)

def safe(root, relative):
    path = Path(relative)
    if path.is_absolute() or '..' in path.parts or ':' in relative or '\\' in relative:
        raise ValueError('Unsafe package path')
    result = root/path
    if not result.resolve().is_relative_to(root.resolve()): raise ValueError('Path escapes package')
    return result

def find_original(directory, spec):
    # Accept author ZIP layouts by verified content, never by an unchecked filename.
    for archive in sorted(directory.glob('*.zip')):
        with zipfile.ZipFile(archive) as z:
            for item in z.infolist():
                if item.file_size != spec['size'] or item.file_size > 64*1024*1024: continue
                if Path(item.filename).name != Path(spec['path']).name: continue
                data=z.read(item)
                if digest(data)==spec['sha256']:return data
    for item in directory.rglob(Path(spec['path']).name):
        if item.is_file() and not item.is_symlink() and item.stat().st_size==spec['size']:
            data=item.read_bytes()
            if digest(data)==spec['sha256']:return data
    raise ValueError('Missing verified original: '+spec['path'])

def server_profile(profile, originals, destination, adapter=None):
    originals, destination=Path(originals),Path(destination)
    if destination.exists():raise ValueError('Use a new staging directory')
    catalog=json.loads((ROOT/'catalog.json').read_text(encoding='utf-8'))
    selection=json.loads((ROOT/f'profiles/{profile}.json').read_text(encoding='utf-8'))
    components={c['id']:c for c in catalog['components']}
    files={}
    for id in selection['server']:
        c=components[id]
        if c['distribution']!='original-import':continue
        for spec in c['files']:
            relative='mods/'+c['folder']+'/'+spec['path'] if c.get('folder') else ('dbghelp.dll' if profile=='normal' else 'GlobalXPShare.original.dll')
            data=find_original(originals,spec)
            if spec['path']=='src/mod.lua' and id in ('rested','auto-loot'):
                name='rested-server' if id=='rested' else 'auto-loot-critters'
                recipe=json.loads((ROOT/'patches/recipes.json').read_text())[name]
                data=transform(recipe,data)
            files[relative]=data
    files['mods/Ember/src/User_Config_Overrides.lua']=(ROOT/'defaults/Ember.lua').read_bytes()
    files['safeprobe_config.ini']=(ROOT/'defaults/safeprobe_config.ini').read_bytes()
    if profile=='cheeze':
        if not adapter:raise ValueError('Cheeze requires its matching dedicated-server adapter')
        files['dbghelp.dll']=Path(adapter).read_bytes()
    # Resolve every original before creating any output. Never touch a game folder.
    for relative in files:safe(destination,relative)
    destination.mkdir(parents=True)
    for relative,data in files.items():
        target=safe(destination,relative);target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    manifest={relative:{'sha256':digest(data),'size':len(data)} for relative,data in files.items()}
    (destination/'prepared-files.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    print('Prepared '+profile+' server files. Apply EMM to matching clean server data before deployment.')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    apply = commands.add_parser('patch')
    apply.add_argument('recipe'); apply.add_argument('input'); apply.add_argument('output')
    hotkey=commands.add_parser('vein-key',help='Set the hold key in a verified XHL 1.0.33 client DLL')
    hotkey.add_argument('input');hotkey.add_argument('output');hotkey.add_argument('--key',required=True,type=lambda value:int(value,0))
    server=commands.add_parser('server')
    server.add_argument('profile',choices=['normal','cheeze'])
    server.add_argument('originals');server.add_argument('output');server.add_argument('--adapter')
    args = parser.parse_args()
    try:
        if args.command=='patch':
            patch(args.recipe, args.input, args.output)
            print('Patch verified successfully. Original preserved.')
        elif args.command=='vein-key':
            source,destination=Path(args.input),Path(args.output)
            if source.resolve()==destination.resolve() or destination.exists():raise ValueError('Use a new separate output file')
            spec=importlib.util.spec_from_file_location('vein_hotkeys',ROOT/'patches/vein-hotkeys.py')
            module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
            manifest=json.loads((ROOT/'patches/vein-hotkey.json').read_text())
            result=module.rekey(source.read_bytes(),args.key,manifest)
            destination.parent.mkdir(parents=True,exist_ok=True)
            with destination.open('xb') as output:output.write(result)
            print('Vein Mining key updated. Original preserved.')
        else:server_profile(args.profile,args.originals,args.output,args.adapter)
        return 0
    except (OSError, ValueError, KeyError) as error:
        print('Patch stopped: ' + str(error), file=sys.stderr)
        return 1

if __name__ == '__main__':
    raise SystemExit(main())
