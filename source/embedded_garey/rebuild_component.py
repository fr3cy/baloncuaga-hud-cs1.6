from pathlib import Path
import hashlib,json,struct,subprocess,re,argparse,shutil,zlib

folder=Path(__file__).resolve().parent
parser=argparse.ArgumentParser(description='Rebuild the reduced x86 CS Fix component from the supplied clean binary baseline.')
parser.add_argument('--bin-dir',type=Path,help='MinGW-w64 bin directory containing i686-w64-mingw32 binutils')
args=parser.parse_args()
def tool(name):
    candidate=args.bin_dir/('i686-w64-mingw32-'+name+'.exe') if args.bin_dir else shutil.which('i686-w64-mingw32-'+name)
    if not candidate:raise SystemExit('Missing i686-w64-mingw32-'+name+'; pass --bin-dir.')
    return str(candidate)
root=folder/'.build';root.mkdir(exist_ok=True)
subprocess.run([tool('as'),'--32',str(folder/'bootstrap_x86.S'),'-o',str(root/'minimal_boot.o')],check=True)
subprocess.run([tool('objcopy'),'-O','binary','-j','.text',str(root/'minimal_boot.o'),str(root/'minimal_boot.bin')],check=True)
packed=(folder/'6000.original.bsk').read_bytes()
assert packed[:4]==b'BSK1'
before=zlib.decompress(packed[8:])
assert len(before)==int.from_bytes(packed[4:8],'little')
assert hashlib.sha256(before).hexdigest()=='7ace1120f2a1595c551d6b0f68e17c2d5167a804a7eae56beac6e48b6a3d4fbd'
data=bytearray(before);pe=struct.unpack_from('<I',data,60)[0];opt=pe+24
count=struct.unpack_from('<H',data,pe+6)[0];table=opt+struct.unpack_from('<H',data,pe+20)[0]
base=struct.unpack_from('<I',data,opt+28)[0];sa,fa=struct.unpack_from('<II',data,opt+32)
sections=[]
for i in range(count):
    virtual,rva,size,raw=struct.unpack_from('<IIII',data,table+i*40+8);sections.append((rva,virtual,raw,size))
def align(n,a):return (n+a-1)//a*a
def raw(va):return next(start+va-base-rva for rva,virtual,start,size in sections if base+rva<=va<base+rva+size)
def original_call(va):
    at=raw(va);assert data[at]==0xe8
    return va+5+struct.unpack_from('<i',data,at+1)[0]
code_rva=align(max(rva+max(v,size) for rva,v,start,size in sections),sa)
state_rva=code_rva+sa
assert code_rva==0xa87000 and state_rva==0xa88000
header=table+count*40;assert header+80<=min(start for rva,v,start,size in sections if size)
assert not any(data[header:header+80])
symbols={}
listing=subprocess.check_output([tool('nm'),'-n',str(root/'minimal_boot.o')],text=True)
for line in listing.splitlines():
    match=re.fullmatch(r'([0-9a-fA-F]+)\s+[Tt]\s+(\w+)',line)
    if match:symbols[match[2]]=int(match[1],16)
relocations=subprocess.check_output([tool('objdump'),'-r',str(root/'minimal_boot.o')],text=True)
assert 'RELOCATION RECORDS' not in relocations
code=bytearray((root/'minimal_boot.bin').read_bytes());assert len(code)<sa
for label,destination in {'pointer_read_call':0x1007f237,'native_fps_call':0x10084534,'original_destructor_call':0x100806e6,'native_voice_call':0x1007ee68,'original_worker_call':0x1009c190}.items():
    at=symbols[label];assert code[at:at+5]==b'\xe8\0\0\0\0'
    struct.pack_into('<i',code,at+1,destination-(base+code_rva+at+5))
assert original_call(0x1009c469)==0x1007f237
assert original_call(0x1009c636)==0x100806e6
assert data[raw(0x10098fa8):raw(0x10098fa8)+5]==bytes.fromhex('6890c10910')
# Preserve the existing HIGHLOW relocation for the replaced absolute worker
# entry, so the normal Windows loader relocates it with the rest of the image.
reloc_rva,reloc_size=struct.unpack_from('<II',before,opt+96+5*8)
reloc_at=raw(base+reloc_rva);reloc_end=reloc_at+reloc_size;relocs=set()
while reloc_at<reloc_end:
    page,length=struct.unpack_from('<II',before,reloc_at)
    assert length>=8 and reloc_at+length<=reloc_end
    for pos in range(reloc_at+8,reloc_at+length,2):
        value=struct.unpack_from('<H',before,pos)[0]
        if value>>12==3:relocs.add(page+(value&4095))
    reloc_at+=length
assert 0x98fa9 in relocs
struct.pack_into('<I',data,raw(0x10098fa8)+1,base+code_rva+symbols['guarded_worker'])
for call,label in ((0x1009c469,'fps_adapter'),(0x1009c636,'voice_adapter')):
    struct.pack_into('<i',data,raw(call)+1,base+code_rva+symbols[label]-call-5)
# The old studio-event pipeline contained the unrelated bounds/console/text/FOV
# patches. It is retired; FPS and voice now start from the captured engine API.
assert data[raw(0x1009a060):raw(0x1009a060)+3]==b'\x55\x8b\xec'
data[raw(0x1009a060)]=0xc3
default=raw(0x101914ec);assert data[default:default+4]==b'150\0';data[default:default+4]=b'100\0'
new_sections=[]
for name,rva,content,flags in ((b'.bfix',code_rva,code,0x60000020),(b'.bfstate',state_rva,b'\0'*16,0xc0000040)):
    start=align(len(data),fa);data.extend(b'\0'*(start-len(data)));data.extend(content);size=align(len(content),fa);data.extend(b'\0'*(size-len(content)))
    record=bytearray(40);record[:8]=name.ljust(8,b'\0');struct.pack_into('<IIII',record,8,len(content),rva,size,start);struct.pack_into('<I',record,36,flags)
    data[header:header+40]=record;header+=40;new_sections.append((rva,len(content),start,size))
struct.pack_into('<H',data,pe+6,count+2)
struct.pack_into('<I',data,opt+4,struct.unpack_from('<I',data,opt+4)[0]+new_sections[0][3])
struct.pack_into('<I',data,opt+8,struct.unpack_from('<I',data,opt+8)[0]+new_sections[1][3])
struct.pack_into('<I',data,opt+56,align(state_rva+16,sa));struct.pack_into('<I',data,opt+64,0)
allowed=set()
for start,length in ((raw(0x1009c469)+1,4),(raw(0x1009c636)+1,4),(raw(0x10098fa8)+1,4),(raw(0x1009a060),1),(default,4),(table+count*40,80),(pe+6,2),(opt+4,8),(opt+56,4),(opt+64,4)):
    allowed.update(range(start,start+length))
diff=[i for i,(a,b) in enumerate(zip(before,data)) if a!=b];assert all(i in allowed for i in diff)
for thunk in (0x10084534,0x1007ee68):
    va=thunk
    while before[raw(va)]==0xe9:va+=5+struct.unpack_from('<i',before,raw(va)+1)[0]
    at=raw(va);end=before.find(b'\xcc'*8,at+1);assert end>=0
    assert data[at:end]==before[at:end]
fnv=1469598103934665603
for b in data:fnv=((fnv^b)*1099511628211)&((1<<64)-1)
expected=json.loads((folder/'manifest.json').read_text('utf-8'))
digest=hashlib.sha256(data).hexdigest()
assert digest==expected['sha256'] and len(data)==expected['bytes'] and hex(fnv)==expected['fnv64'], 'Output differs from the supplied component; no resource overwritten.'
packed=b'BSK1'+struct.pack('<I',len(data))+zlib.compress(data,9)
assert len(packed)==expected['packed']
(root/'cs_fix.asi').write_bytes(data)
(folder/'6000.bsk').write_bytes(packed)
print(json.dumps({'sha256':digest,'bytes':len(data),'packed':len(packed),'feature_bodies_unchanged':True,'default_fps_visual':100}))
