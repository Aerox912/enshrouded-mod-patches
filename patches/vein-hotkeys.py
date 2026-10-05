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
    offset=manifest['inputImportOffset']; original=manifest['inputImportOriginal'].encode()+b'\0'; proxy=manifest['inputImportProxy'].encode()+b'\0'
    if len(original)!=len(proxy) or result[offset:offset+len(original)] not in (original,proxy): raise ValueError('Unsupported Vein Mining input import')
    result[offset:offset+len(original)]=original
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

def rekey(data,key,manifest=None,mode='hold'):
    if not isinstance(key,int) or not 1<=key<=254:raise ValueError('Expected one Windows virtual-key code from 1 to 254')
    manifest=manifest or json.loads(Path(__file__).with_name('vein-hotkey.json').read_text())
    if mode not in ('hold','toggle','always'):raise ValueError('Expected hold, toggle or always')
    result,old=normalize(data,manifest);result=bytearray(result)
    for site in manifest['sites']:struct.pack_into('<I',result,site['offset']+1,key)
    if mode!='hold':
        offset=manifest['inputImportOffset'];name=manifest['inputImportProxy'].encode()+b'\0';result[offset:offset+len(name)]=name
    return bytes(result)
