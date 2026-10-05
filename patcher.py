"""Verified local transformations. Original mods are never bundled or overwritten."""
import argparse
import base64
import hashlib
import importlib.util
import json
import re
import sys
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

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    apply = commands.add_parser('patch')
    apply.add_argument('recipe'); apply.add_argument('input'); apply.add_argument('output')
    args = parser.parse_args()
    try:
        patch(args.recipe, args.input, args.output)
        print('Patch verified successfully. Original preserved.')
        return 0
    except (OSError, ValueError, KeyError) as error:
        print('Patch stopped: ' + str(error), file=sys.stderr)
        return 1

if __name__ == '__main__':
    raise SystemExit(main())
