"""English preview strings for the pinned XHL Vein Mining 1.0.33 client DLL.

Adds read-only UTF-16 strings and retargets only verified RIP-relative LEAs.
Requires Python 3; never edits the source DLL. No game code or material IDs change.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path


def localize(source, destination, manifest_path):
    source, destination = Path(source), Path(destination)
    if source.resolve() == destination.resolve():
        raise ValueError('Use a separate output path; preserve the original DLL')
    manifest = json.loads(Path(manifest_path).read_text(encoding='utf-8'))
    original = source.read_bytes()
    if hashlib.sha256(original).hexdigest() != manifest['sha256']:
        raise ValueError('Expected original XHL Vein Mining 1.0.33 DLL; refusing this version')
    output = bytearray(original)
    pe = struct.unpack_from('<I', original, 0x3c)[0]
    optional = pe + 24
    sections = struct.unpack_from('<H', original, pe + 6)[0]
    section_table = optional + struct.unpack_from('<H', original, pe + 20)[0]
    header = section_table + sections * 40
    section_alignment, file_alignment = struct.unpack_from('<II', original, optional + 32)
    size_headers = struct.unpack_from('<I', original, optional + 60)[0]
    align = lambda size, alignment: (size + alignment - 1) // alignment * alignment
    if header + 40 > size_headers or any(original[header:header + 40]):
        raise ValueError('No unused section header')
    if struct.unpack_from('<II', original, optional + 112 + 4 * 8) != (0, 0):
        raise ValueError('Unexpected certificate directory')
    last = section_table + (sections - 1) * 40
    virtual_size, virtual_address, raw_size, raw_address = struct.unpack_from('<IIII', original, last + 8)
    if raw_address + raw_size != len(original):
        raise ValueError('Unexpected trailing data')
    new_rva = align(virtual_address + virtual_size, section_alignment)
    new_raw = align(len(original), file_alignment)
    strings = bytearray()
    destinations = {}
    for entry in manifest['strings']:
        before = (entry['original'] + '\0').encode('utf-16le')
        if original[entry['offset']:entry['offset'] + len(before)] != before:
            raise ValueError('Source string mismatch')
        destinations[entry['rva']] = new_rva + len(strings)
        strings.extend((entry['english'] + '\0').encode('utf-16le'))
    for ref in manifest['references']:
        offset = ref['offset']
        expected = bytes.fromhex(ref['bytes'])
        if original[offset:offset + len(expected)] != expected:
            raise ValueError('Source instruction mismatch')
        delta = destinations[ref['target']] - (ref['rva'] + ref['size'])
        struct.pack_into('<i', output, offset + ref['displacement'], delta)
    padded = align(len(strings), file_alignment)
    struct.pack_into('<8sIIIIIIHHI', output, header,
                     b'.enui\0\0\0', len(strings), new_rva, padded, new_raw, 0, 0, 0, 0, 0x40000040)
    struct.pack_into('<H', output, pe + 6, sections + 1)
    initialized = struct.unpack_from('<I', original, optional + 8)[0]
    struct.pack_into('<I', output, optional + 8, initialized + padded)
    struct.pack_into('<I', output, optional + 56, align(new_rva + len(strings), section_alignment))
    # User-mode DLLs do not require a checksum. Clear the original checksum.
    struct.pack_into('<I', output, optional + 64, 0)
    output.extend(bytes(new_raw - len(output)))
    output.extend(strings)
    output.extend(bytes(padded - len(strings)))
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(output)
    print(json.dumps({'output': str(destination), 'strings': len(destinations),
                      'references': len(manifest['references']), 'bytes': len(output),
                      'sha256': hashlib.sha256(output).hexdigest()}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    parser.add_argument('--manifest', type=Path, default=Path(__file__).with_name('vein-english.json'))
    args = parser.parse_args()
    localize(args.source, args.destination, args.manifest)
