"""Prepare the paired radius DLLs from verified local XHL originals. MIT.

This stages separate files only. It never installs, starts a process, copies
personal settings, or redistributes the original author's DLL.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('vein_hotkeys', ROOT / 'patches/vein-hotkeys.py')
HOTKEYS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(HOTKEYS)
SERVER_ORIGINAL = '6d31c439fda536aad2078683a244e2d05f685e5e3fba2d5467950fff613f5bcc'
SERVER_EXPANDED = '4921a0009dbb1f9fcca79be355a4e3fb70481be4b811b1f1eba1c4bcbab936f7'
DLL_PATH = 'mods/XHL-Vein-Mining/XHL-Vein-Mining.dll'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def prepare(client, server, output, mode, key=None):
    client, server, output = Path(client), Path(server), Path(output)
    if output.exists():
        raise ValueError('Use a new staging directory; existing files are never replaced')
    # Bound reads before parsing. Known original and localized files only.
    for path in (client, server):
        if path.stat().st_size not in (290816, 293888):
            raise ValueError('Unsupported XHL 1.0.33 DLL length')
    client_data, server_data = client.read_bytes(), server.read_bytes()
    manifest = json.loads((ROOT / 'patches/vein-hotkey.json').read_text())
    _, saved_key = HOTKEYS.normalize(client_data, manifest)
    if digest(server_data) not in (SERVER_ORIGINAL, SERVER_EXPANDED):
        raise ValueError('Server requires the pinned official original or expanded DLL')
    files = {
        'client/' + DLL_PATH: HOTKEYS.enable_radius(client_data, saved_key if key is None else key, manifest, mode),
        'server/' + DLL_PATH: HOTKEYS.expand_capacity(server_data, manifest),
    }
    if digest(files['server/' + DLL_PATH]) != SERVER_EXPANDED:
        raise ValueError('Expanded server output does not match the paired protocol')
    # Resolve and validate both inputs before creating any output.
    output.mkdir(parents=True, exist_ok=False)
    for name, data in files.items():
        target = output / name
        target.parent.mkdir(parents=True, exist_ok=True)
        with target.open('xb') as stream:
            stream.write(data)
    record = {'schema': 1, 'component': 'vein-radius', 'version': '1.2.0',
              'protocol': 'A6', 'mode': mode, 'activationKey': saved_key if key is None else key,
              'inputs': {'client': digest(client_data), 'server': digest(server_data)},
              'files': {name: {'sha256': digest(data), 'size': len(data)} for name, data in files.items()}}
    (output / 'prepared-files.json').write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client', type=Path, required=True, help='Verified original or localized client DLL')
    parser.add_argument('--server', type=Path, required=True, help='Verified official server DLL')
    parser.add_argument('--output', type=Path, required=True, help='New staging directory')
    parser.add_argument('--mode', choices=('hold', 'toggle', 'always'), default='toggle', help='Install/update preset: toggle; other modes are manual overrides')
    parser.add_argument('--key', type=lambda value: int(value, 0), help='Windows virtual-key code; preserve current key if omitted')
    args = parser.parse_args()
    try:
        prepare(args.client, args.server, args.output, args.mode, args.key)
    except (OSError, ValueError, KeyError) as error:
        parser.exit(1, 'Preparation stopped: ' + str(error) + '\n')
    print('Prepared matching DLLs. Install only with the pinned helper/server pair; set controls.ini Mode=' + args.mode + ' and preserve the activation key and radius.ini.')


if __name__ == '__main__':
    main()
