from pathlib import Path
import argparse,hashlib,json
p=argparse.ArgumentParser();p.add_argument('--models',type=Path,required=True);p.add_argument('--header',type=Path,required=True);p.add_argument('--manifest',type=Path,required=True);a=p.parse_args()
models=json.loads((a.models/'native_manifest.json').read_text('utf-8'))['models'];rows=[]
order=[('urban',2,'ct'),('gign',2,'gign'),('gsg9',2,'gsg9'),('sas',2,'sas'),('vip',2,'vip'),('terror',1,'tt'),('leet',1,'leet'),('arctic',1,'arctic'),('guerilla',1,'guerilla')]
for native,side,key in order:
 d=(a.models/(key+'.mdl')).read_bytes();h=1469598103934665603
 for b in d:h=((h^b)*1099511628211)&0xffffffffffffffff
 rec=models[key];assert hashlib.sha256(d).hexdigest()==rec['sha256'];rec['fnv64']=f'{h:016x}'
 rows.append('{"'+native+'","models/baloncuaga_outfits/'+key+'.mdl",'+str(side)+','+str(rec['bones'])+',UINT64_C(0x'+rec['fnv64']+')}')
a.header.write_text('#pragma once\nstruct OutfitAsset {const char *native,*file;int side,bones;uint64_t hash;};\nstatic const OutfitAsset assets[]={\n'+',\n'.join(rows)+'\n};\nstatic constexpr int assetCount=sizeof(assets)/sizeof(assets[0]);\n','utf-8')
a.manifest.write_text(json.dumps({'format':2,'visual_only':True,'models':models},indent=2)+'\n','utf-8')
