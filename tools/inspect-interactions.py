"""Read-only layout evidence for the standalone harvesting/looting mods.

Requires pefile. Reads the user's executable without modifying it. Reports only
type/field metadata and named-system addresses, never game assets or save data.
"""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

import pefile


def inspect(path):
    pe = pefile.PE(str(path))
    image = pe.get_memory_mapped_image()
    base = pe.OPTIONAL_HEADER.ImageBase

    def string(address):
        offset = address - base
        if not 0 <= offset < len(image):
            return ""
        raw = image[offset:offset + 256].split(b"\0", 1)[0]
        return raw.decode() if re.fullmatch(rb"[\x20-\x7e]{1,255}", raw) else ""

    def pointer(offset):
        return struct.unpack_from("<Q", image, offset)[0]

    def references(offset):
        return [m.start() for m in re.finditer(re.escape(struct.pack("<Q", base + offset)), image)]

    types = []
    enums = []
    for match in re.finditer(rb"keen::(?:(?:ecs|actor)::)?[A-Za-z0-9_]+\0", image):
        name = match.group()[:-1].decode()
        if not re.search(r"Loot|Death|Dead|Damage|Kill|Interaction|Inventory|Growth|Player|EntityId|Template|Health|Impact|Experience|Target|Hit|Owner|Combat|Cursor|Input|VersionedData|RenderTransform|UiInteractable|GameObject|ActionSequence", name):
            continue
        for reference in references(match.start()):
            descriptor = reference - 32
            if descriptor < 0 or pointer(reference + 8) != len(name):
                continue
            size = struct.unpack_from("<I", image, descriptor + 64)[0]
            count = struct.unpack_from("<I", image, descriptor + 72)[0]
            fields_offset = pointer(descriptor + 88) - base
            # Enum descriptors have 40-byte name/value entries in a separate table.
            # Read explicit values instead of assuming declaration order.
            if fields_offset == -base and size in (1, 2, 4, 8) and 0 < count <= 128:
                enum_offset = pointer(descriptor + 96) - base
                if 0 <= enum_offset <= len(image) - count * 40:
                    values = []
                    for index in range(count):
                        entry = enum_offset + index * 40
                        item_name = string(pointer(entry))
                        if not item_name or pointer(entry + 8) != len(item_name):
                            break
                        values.append(dict(name=item_name, value=pointer(entry + 16)))
                    if len(values) == count:
                        enums.append(dict(name=name, descriptor=descriptor, size=size, values=values))
            if size > 0x100000 or count > 128 or not 0 <= fields_offset < len(image) - count * 48:
                continue
            fields = []
            for index in range(count):
                entry = fields_offset + index * 48
                field_name = string(pointer(entry))
                field_type = pointer(entry + 16) - base
                offset = pointer(entry + 24)
                type_name = string(pointer(field_type + 32)) if 0 <= field_type < len(image) - 40 else ""
                if not field_name or offset > size:
                    break
                fields.append(dict(name=field_name, offset=offset, type=type_name))
            if len(fields) == count:
                types.append(dict(name=name, descriptor=descriptor, size=size, fields=fields))

    systems = []
    for match in re.finditer(rb"[a-z][a-z0-9_]{5,100}\0", image):
        name = match.group()[:-1].decode()
        if not re.search(r"interaction|loot|death|kill|damage|inventory|growth|experience|impact", name):
            continue
        for reference in references(match.start()):
            callback = pointer(reference + 16) - base
            if pointer(reference + 8) != len(name):
                continue
            if any(s.Characteristics & 0x20000000 and s.VirtualAddress <= callback < s.VirtualAddress + s.Misc_VirtualSize for s in pe.sections):
                systems.append(dict(name=name, descriptor=reference, callback=callback))
    return dict(executable=path.name, sha256=hashlib.sha256(path.read_bytes()).hexdigest(), types=types, enums=enums, systems=systems)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = json.dumps(inspect(args.executable), indent=2)
    if args.output:
        args.output.write_text(result + "\n", encoding="utf-8")
    else:
        print(result)
