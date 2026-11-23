"""Repack project-owned game resources and outfits for automatic installation.

Run from any directory after updating the resource/model manifests. No local
settings, user models, executables, logs or credentials are collected.
"""
from pathlib import Path
import hashlib,json,struct,zlib

root=Path(__file__).resolve().parents[2]
dest=root/'source/embedded_install';dest.mkdir(exist_ok=True)
inputs=[]
for p in sorted((root/'game_skin').rglob('*')):
    if p.is_file() and p.name!='glass_scoreboard.tga':
        inputs.append((p.relative_to(root/'game_skin').as_posix(),p,True))
for p in sorted((root/'assets/cstrike/models/baloncuaga_outfits').glob('*.mdl')):
    inputs.append(('cstrike/models/baloncuaga_outfits/'+p.name,p,False))
assert len(inputs)==120
rows=[];resources=[];manifest=[]
for i,(name,path,restart) in enumerate(inputs):
    assert ':' not in name and '..' not in name.split('/')
    assert path.suffix.lower() in ('.mdl','.res','.tga','.txt')
    raw=path.read_bytes();assert 18<=len(raw)<=32*1024*1024
    h=1469598103934665603
    for b in raw:h=((h^b)*1099511628211)&0xffffffffffffffff
    packed=b'BSK1'+struct.pack('<I',len(raw))+zlib.compress(raw,9)
    rid=5100+i;filename=str(rid)+'.bsk';(dest/filename).write_bytes(packed)
    rows.append('{'+str(rid)+',"'+name+'",'+str(len(raw))+',UINT64_C(0x'+f'{h:016x}'+'),'+('true' if restart else 'false')+'}')
    resources.append(f'{rid} RCDATA "{filename}"')
    manifest.append(dict(id=rid,path=name,file=filename,bytes=len(raw),packed=len(packed),sha256=hashlib.sha256(raw).hexdigest(),fnv64=f'{h:016x}',restart=restart))
bundle=hashlib.sha256(json.dumps(manifest,sort_keys=True).encode()).hexdigest()[:16]
(dest/'files.rc').write_text('\n'.join(resources)+'\n','ascii')
(dest/'files.h').write_text('#pragma once\nstruct File {int id;const char *path;size_t bytes;uint64_t hash;bool restart;};\nstatic const char *bundle="'+bundle+'";\nstatic const File files[]={\n'+',\n'.join(rows)+'\n};\n','utf-8')
(dest/'manifest.json').write_text(json.dumps({'bundle':bundle,'files':manifest},indent=2)+'\n','utf-8')
print('Packed',len(manifest),'manifest-owned files. Rebuild ASI next.')
