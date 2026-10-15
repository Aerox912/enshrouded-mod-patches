"""Rebind the three pinned XHL 1.0.33 client hold-key reads. MIT, Aerox912.

This descriptor covers local mining, multiplayer requests and the cursor preview.
Normalizing the key, input import and optional bounded radius capacity must
reproduce a known complete DLL checksum. Radius support requires the paired
native helper/server extension; stock inputs retain their previous behavior.
"""
import hashlib,json,struct
from pathlib import Path

def normalize(data, manifest):
    result=bytearray(data); keys=[]
    if len(result)<11989 or struct.unpack_from('<I',result,11985)[0] not in (65536,262144):
        raise ValueError('Unsupported Vein Mining region capacity')
    struct.pack_into('<I',result,11985,65536)
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
    radius=struct.unpack_from('<I',data,11985)[0]==262144
    if radius and key==0x7b:raise ValueError('F12 is reserved for the Vein Mining radius selector')
    if radius:struct.pack_into('<I',result,11985,262144)
    for site in manifest['sites']:struct.pack_into('<I',result,site['offset']+1,key)
    if mode!='hold' or radius:
        offset=manifest['inputImportOffset'];name=manifest['inputImportProxy'].encode()+b'\0';result[offset:offset+len(name)]=name
    return bytes(result)

def expand_capacity(data,manifest=None):
    """Expand only the verified 64^3 region budget, before the module is loaded."""
    manifest=manifest or json.loads(Path(__file__).with_name('vein-hotkey.json').read_text())
    normalize(data,manifest)
    result=bytearray(data);struct.pack_into('<I',result,11985,262144)
    return bytes(result)

def enable_radius(data,key,manifest=None,mode='hold'):
    return rekey(expand_capacity(data,manifest),key,manifest,mode)
