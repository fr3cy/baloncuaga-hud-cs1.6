"""Technical export of finished baLONCUaga artwork into stock GoldSrc assets.
No scene painting, compositing of artwork, retouching or generated lettering.
The background is contained intact, then resampled for native 800x600 scaling.
"""
from pathlib import Path
from PIL import Image,ImageOps
import argparse,hashlib,json,struct,shutil
parser=argparse.ArgumentParser();parser.add_argument('--review-dir',type=Path);args=parser.parse_args()
root=Path(__file__).resolve().parent.parent;skin=root/'game_skin';brand=root/'assets/branding';art=root/'artwork'
master=brand/'baloncuaga_3xl_main_background.png';shoe_path=brand/'baloncuaga_3xl_cutout.png';wordmark_path=brand/'wordmark_native.png'
for p in (master,shoe_path,wordmark_path):
 if not p.is_file():raise SystemExit('Required project artwork is missing: '+p.name)
source=Image.open(master).convert('RGB');shoe=Image.open(shoe_path).convert('RGBA');wordmark=Image.open(wordmark_path).convert('RGBA');assert wordmark.size==(512,32)
canonical=(skin/'cstrike/resource/BackgroundLayout.txt').read_bytes();assert canonical.startswith(b'resolution\t800\t600');assert canonical.count(b'\\t')==0
footer=b'\0'*8+b'TRUEVISION-XFILE.\0';headers={}
for row in range(1,4):
 for col in 'abcd':
  name=f'800_{row}_{col}_loading.tga';raw=(skin/'cstrike/resource/background'/name).read_bytes();assert raw[-26:]==footer;headers[name]=raw[:18]
logo_header=(skin/'cstrike/resource/logo_game.tga').read_bytes()[:18];icon_header=(skin/'cstrike/gfx/vgui/CS_logo.tga').read_bytes()[:18]
def safe(path):
 p=path.resolve();assert p.is_relative_to(root.resolve()),'export must stay inside the project';return p
def tga(path,image,header):
 path=safe(path);path.parent.mkdir(parents=True,exist_ok=True);alpha=image.mode=='RGBA';w,h=image.size;v=struct.unpack('<BBBHHBHHHHBB',header);assert v[2]==2 and v[8:11]==(w,h,32 if alpha else 24)
 data=header+image.transpose(Image.Transpose.FLIP_TOP_BOTTOM).tobytes('raw','BGRA' if alpha else 'BGR')+footer;path.write_bytes(data)
 check=Image.open(path).convert(image.mode);assert check.tobytes()==image.tobytes()
# Required 64x64 native icon: only alpha-bounds normalization and containment.
bounds=shoe.getchannel('A').getbbox();assert bounds;cut=shoe.crop(bounds);small=ImageOps.contain(cut,(60,60),Image.Resampling.LANCZOS);icon=Image.new('RGBA',(64,64),(0,0,0,0));icon.paste(small,((64-small.width)//2,(64-small.height)//2))
icon.save(art/'cs_logo.png');wordmark.save(art/'logo_game.png')
variants={};review=args.review_dir
if review:review.mkdir(parents=True,exist_ok=True)
for aspect,size in [('5_4',(1280,1024)),('4_3',(1600,1200)),('16_9',(1600,900))]:
 fitted=ImageOps.contain(source,size,Image.Resampling.LANCZOS);x=(size[0]-fitted.width)//2;y=(size[1]-fitted.height)//2;canvas=Image.new('RGB',size,(5,5,6));canvas.paste(fitted,(x,y));native=canvas.resize((800,600),Image.Resampling.LANCZOS)
 target=skin if aspect=='5_4' else root/'background_variants'/aspect
 for mod in ['cstrike','valve']:
  res=target/mod/'resource';background=res/'background';background.mkdir(parents=True,exist_ok=True)
  for old in list(background.glob('baloncuaga_*.tga')):safe(old).unlink()
  for filename in ['BackgroundLayout.txt','BackgroundLoadingLayout.txt']:(res/filename).write_bytes(canonical)
  for row,sy in enumerate((0,256,512),1):
   for col,sx in enumerate((0,256,512,768)):
    name=f'800_{row}_{"abcd"[col]}_loading.tga';tile=native.crop((sx,sy,min(sx+256,800),min(sy+256,600)));tga(background/name,tile,headers[name])
  tga(res/'logo_game.tga',wordmark,logo_header);tga(target/mod/'gfx/vgui/CS_logo.tga',icon,icon_header)
 variants[aspect]={'path':target.relative_to(root).as_posix(),'display_size':list(size),'contained_art_size':list(fitted.size),'padding_offset':[x,y],'native_resolution':[800,600],'tiles_per_module':12,'modules':['cstrike','valve'],'crop':False}
 if review:
  canvas.save(review/f'background_fit_{aspect}.png');native.save(review/f'background_native_{aspect}.png');native.resize(size,Image.Resampling.BILINEAR).save(review/f'background_display_{aspect}.png')
# Internal native-scheme colour aliases are renamed consistently; property
# values, font definitions, layout geometry and native commands stay identical.
alias_changes=[]
for p in sorted(skin.rglob('*.res')):
 original=p.read_bytes()
 if b'Moon' not in original:continue
 changed=original.replace(b'MoonPressedBG',b'BrandPressedBG').replace(b'MoonSlider',b'BrandSlider').replace(b'MoonFrameBorder',b'BrandFrameBorder').replace(b'MoonScoreboard',b'BrandScoreboard')
 if changed!=original:p.write_bytes(changed)
 if any(k in changed for k in (b'BrandPressedBG',b'BrandSlider',b'BrandFrameBorder',b'BrandScoreboard')):alias_changes.append(p.relative_to(skin).as_posix())
for old in [art/'carbon_moon_master.png']:
 if old.exists():safe(old).unlink()
(art/'BACKGROUND_PROMPT.txt').write_text('Original finished monochrome technical-runner studio background and transparent shoe illustration, generated with the built-in image_gen tool. Exact custom lettering: baLONCUaga. Sources are project assets under assets/branding/. Native export performs only containment, neutral padding, required resampling, format conversion and tiling. The separate native wordmark is code-native typography.\n','utf-8')
files=[]
for parent in [skin,root/'background_variants']:
 for p in sorted(parent.rglob('*')):
  if p.is_file() and (p.suffix.lower()=='.tga' or p.name in ('BackgroundLayout.txt','BackgroundLoadingLayout.txt')):files.append({'path':p.relative_to(root).as_posix(),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
provenance={'brand':'baLONCUaga','artwork_sources':[{'path':p.relative_to(root).as_posix(),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'origin':'built_in_image_gen' if p!=wordmark_path else 'code_native_typography'} for p in [master,shoe_path,wordmark_path]],'wordmark_vector':'assets/branding/wordmark_native.svg','artwork_edits':'none; technical format/size exports only','native_export':{'stock_layout':'800x600 / 12 native tiles','native_headers':'unchanged','native_footer':'TRUEVISION-XFILE','wordmark':[512,32],'shoe_icon':[64,64]},'variants':variants,'scheme_alias_files':alias_changes,'game_files_modified':False,'native_runtime_verified':False,'files':files}
(art/'BRANDING_PROVENANCE.json').write_text(json.dumps(provenance,ensure_ascii=False,indent=2),'utf-8')
# Keep the native resource manifests in sync with the exported payload while
# preserving their existing structural and functional validation metadata.
for filename,entries_key in [('RESOURCE_MANIFEST.json','files'),('VGUI_THEME_MANIFEST.json','entries')]:
 path=root/filename
 if not path.exists():continue
 manifest=json.loads(path.read_text('utf-8-sig'));manifest['theme']='baLONCUaga 3XL native UI'
 for entry in manifest[entries_key]:
  resource=skin/entry['path'];raw=resource.read_bytes();entry['bytes']=len(raw);entry['sha256']=hashlib.sha256(raw).hexdigest()
 path.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n','utf-8')
if review:(review/'native_branding_manifest.json').write_text(json.dumps(provenance,ensure_ascii=False,indent=2),'utf-8')
print('Exported:',len(files),'native background/layout/logo files; 3 aspect ratios; intact source art; exact baLONCUaga wordmark.')
