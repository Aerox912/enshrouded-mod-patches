"""Rebind the three pinned XHL 1.0.33 client hold-key reads. MIT, Aerox912.

This descriptor covers local mining, multiplayer requests and the cursor preview.
Only the immediate virtual-key argument to GetAsyncKeyState changes. Normalizing
it back to VK_OEM_3 must reproduce a known complete DLL checksum. No original DLL
is distributed, no additional hook is installed, and server bytes are untouched.
"""
import hashlib,json,struct
from pathlib import Path

def normalize(data, manifest):
    result=bytearray(data); keys=[]
    for site in manifest['sites']:
        offset=site['offset'];expected=bytes.fromhex(site['bytes'])
        if offset < 0 or offset+len(expected)>len(data): raise ValueError('Unsupported Vein Mining DLL length')
        actual=bytearray(data[offset:offset+len(expected)])
        keys.append(struct.unpack_from('<I',actual,1)[0])
        struct.pack_into('<I',actual,1,manifest['defaultKey'])
        if actual != expected:raise ValueError('Vein Mining instruction mismatch')
        result[offset:offset+len(expected)]=actual
    if len(set(keys))!=1 or not 1<=keys[0]<=254:raise ValueError('Inconsistent Vein Mining keys')
    if hashlib.sha256(result).hexdigest() not in manifest['hashes']:raise ValueError('Unsupported Vein Mining version or modified DLL')
    return bytes(result),keys[0]

def rekey(data,key,manifest=None):
    if not isinstance(key,int) or not 1<=key<=254:raise ValueError('Expected one Windows virtual-key code from 1 to 254')
    manifest=manifest or json.loads(Path(__file__).with_name('vein-hotkey.json').read_text())
    result,old=normalize(data,manifest);result=bytearray(result)
    for site in manifest['sites']:struct.pack_into('<I',result,site['offset']+1,key)
    return bytes(result)
