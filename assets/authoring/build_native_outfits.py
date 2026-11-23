"""Clothing on each stock CS1.6 player mesh. Keeps native heads, hands and rigs.
Each indexed skin is at most512x512; no source photographs are embedded."""
from pathlib import Path
import argparse,hashlib,json,math,struct
import numpy as np
from PIL import Image
import outfit_tools as t
import studio_strips

VARIANTS=[('urban','ct','ct'),('gign','ct','gign'),('gsg9','ct','gsg9'),('sas','ct','sas'),
          ('vip','ct','vip'),('terror','tt','tt'),('leet','tt','leet'),('arctic','tt','arctic'),('guerilla','tt','guerilla')]
UP={'front':(0,0,320,352),'back':(320,0,192,352),'sleeve':(0,352,256,160),'hood':(256,352,160,160)}
LOW={'left':(0,0,170,512),'right':(170,0,170,512),'legback':(340,0,172,512)}
SHOE={'outer':(0,0,256,320),'vamp':(256,0,256,256),'tongue':(256,256,256,256),'sole':(0,320,256,160)}
SWATCH_UP={n:(436+(j%3)*27,371+(j//3)*40) for j,n in enumerate(t.SWATCH)}
SOURCE_RECTS=dict(t.RECTS);SOURCE_SWATCH=dict(t.SWATCH)
SWATCH_LO={n:(281+(j%4)*57,350+(j//4)*70) for j,n in enumerate(t.SWATCH)}

def palette(im):return im.quantize(colors=256,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE)
def textures(team):
    # High-resolution material source is authoring input only. Three separate
    # 512 indexed atlases keep the engine's indexed->RGBA scratch buffer safe.
    original=Image.open(Path(__file__).resolve().parents[1]/'materials'/(team+'_panels.png')).convert('RGB')
    def panel(row,col):
        w,h=original.size
        return original.crop((col*w//3+2,row*h//3+2,(col+1)*w//3-2,(row+1)*h//3-2))
    def part(im,box):
        w,h=im.size;return im.crop(tuple(int(v*(w if i%2==0 else h)) for i,v in enumerate(box)))
    patches=[{'front':panel(0,0),'back':panel(0,1),
              'sleeve':part(panel(0,2),(0,.53,1,.99)),
              'hood':part(panel(0,1),(.18,.04,.82,.72))},
             {'left':panel(1,0),'right':panel(1,1),'legback':panel(1,2)},
             {'outer':panel(2,0),'vamp':part(panel(2,1),(0,0,1,.85)),'tongue':panel(2,2),
              'sole':part(panel(2,0),(0,.87,1,.99))}]
    outputs=[]
    for charts,patch in zip((UP,LOW,SHOE),patches):
        im=Image.new('RGB',(512,512),(24,25,27))
        for key,(x,y,w,h) in charts.items():
            im.paste(patch[key].resize((w,h),Image.Resampling.LANCZOS),(x,y))
        if charts is UP:
            for key,(x,y) in SWATCH_UP.items():
                color={'black':(17,18,20),'dark':(24,25,27),'silver':(126,126,123),'cream':(193,184,160),'red':(151,18,30),'stitch':(79,80,82),'skin':(100,81,64)}[key]
                im.paste(color,(x-8,y-8,x+9,y+9))
        outputs.append(palette(im))
    return outputs

def smooth_cloth(mesh,protected,upper,lower):
    """Bounded silhouette smoothing, locking skin and clothing boundaries.

    No subdivisions or new rig weights. The stock vertex/bone relationships
    and all native animations remain unchanged.
    """
    faces=list(mesh.faces);old=np.array(mesh.points);edges={};adj={};cloth=set()
    for fi,(mat,ids,uvs) in enumerate(faces):
        if mat not in (upper,lower):continue
        cloth.add(fi)
        for j in range(3):
            a,b,c=ids[j],ids[(j+1)%3],ids[(j+2)%3]
            edges.setdefault(tuple(sorted((a,b))),[]).append(c)
            adj.setdefault(a,set()).add(b);adj.setdefault(b,set()).add(a)
    locked=set(protected)
    for (a,b),op in edges.items():
        if len(op)!=2:locked.update((a,b))
    for i,neighbours in adj.items():
        if i in locked:continue
        n=len(neighbours);beta=3/(8*n) if n>3 else 3/16
        smooth=(1-n*beta)*old[i]+beta*old[list(neighbours)].sum(axis=0)
        mesh.points[i]=old[i]*.30+smooth*.70
    # Smooth existing garment vertices and normals without adding a midpoint
    # to every edge. Native skin/head corners keep their original normals.
    mesh.corner_data={fi:corners for fi,corners in mesh.corner_data.items() if fi not in cloth}


def tex(charts,name,u,v):return t.uv(charts[name],u,v)
def descendants(base,bone):
    result={bone}
    for i,b in enumerate(base.bones):
        if b['parent'] in result:result.add(i)
    return result

def create(base,team):
    m=t.Mesh(base);m.corner_data={};source=base.body_model();h=base.h;data=base.data
    vertices=np.frombuffer(data,'<f4',source['verts']*3,source['vertindex']).reshape(-1,3)
    vb=np.frombuffer(data,np.uint8,source['verts'],source['vertbone'])
    norms=np.frombuffer(data,'<f4',source['norms']*3,source['normindex']).reshape(-1,3)
    nb=np.frombuffer(data,np.uint8,source['norms'],source['normbone'])
    names={b['name']:i for i,b in enumerate(base.bones)}
    head=names['Bip01 Head'];heads=descendants(base,head)
    hands=descendants(base,names['Bip01 L Hand'])|descendants(base,names['Bip01 R Hand'])
    hands|={i for i,b in enumerate(base.bones) if 'wrist' in b['name'].lower() or 'knuckle' in b['name'].lower()}
    original=np.array([base.bind[int(b)][:3,:3]@v+base.bind[int(b)][:3,3] for v,b in zip(vertices,vb)])
    # Stock holsters, chest radios and armor pouches are disconnected islands.
    # They do not belong to these civilian garments; retaining them produces
    # floating rectangular slabs instead of fabric at the hip and chest.
    neighbours={i:set() for i in range(len(original))}
    for _,cs in base.triangles(source):
        ids=[c[0] for c in cs]
        for i in ids:neighbours[i].update(ids)
    unseen=set(neighbours);accessories=set()
    while unseen:
        todo=[next(iter(unseen))];island=set()
        while todo:
            i=todo.pop()
            if i in island:continue
            island.add(i);todo.extend(neighbours[i]-island)
        unseen-=island
        if len(island)<64 and max(original[i][2] for i in island)<58 and not any(int(vb[i]) in heads|hands for i in island):accessories.update(island)
    cx,cy,cz=base.bind[names['Bip01 Pelvis']][:3,3];upper=h['numskinref'];lower=upper+1;shoe=upper+2
    protected=set()
    for i,(point,b) in enumerate(zip(original,vb)):
        point=point.copy();bn=base.bones[int(b)]['name'].lower()
        if int(b) in heads or int(b) in hands:protected.add(i)
        else:
            if 'thigh' in bn or 'calf' in bn or 'knee' in bn or 'butt' in bn or (point[2]<37 and 'pelvis' in bn):
                side='L' if ' l ' in bn else 'R' if ' r ' in bn else ('L' if point[0]>cx else 'R')
                thigh=base.bind[names[f'Bip01 {side} Thigh']][:3,3];ankle=base.bind[names[f'Bip01 {side} Foot']][:3,3]
                v=np.clip((point[2]-ankle[2])/(thigh[2]-ankle[2]),0,1);center=ankle*(1-v)+thigh*v
                spread=1.12 if team=='ct' else 1.04+.14*max(0,min(1,(21-point[2])/16))
                point[0]=center[0]+(point[0]-center[0])*spread;point[1]=center[1]+(point[1]-center[1])*(1.07 if team=='ct' else 1.02)
            elif 'foot' in bn or 'toe' in bn or 'ankle' in bn:
                center=base.bind[names['Bip01 L Foot' if ' l ' in bn else 'Bip01 R Foot']][:3,3]
                point[:2]=center[:2]+(point[:2]-center[:2])*(1.05 if team=='ct' else 1.025)
            elif point[2]>36:
                point[0]=cx+(point[0]-cx)*1.02;point[1]=cy+(point[1]-cy)*1.04
        m.vertex(point,int(b))
    kept=0
    for material,corners in base.triangles(source):
        ids=tuple(c[0] for c in corners);points=np.array([original[i] for i in ids]);avg=points.mean(axis=0)
        if all(i in accessories for i in ids):continue
        preserve=any(int(vb[i]) in heads for i in ids) or all(int(vb[i]) in hands for i in ids)
        # Replace the native low-poly boot with a rounded dedicated shoe mesh.
        boot=sum(any(a in base.bones[int(vb[i])]['name'].lower() for a in ('foot','toe','ankle')) for i in ids)>=2
        if boot and not preserve:continue
        if preserve:mat=material;coords=tuple((c[2],c[3]) for c in corners);kept+=1
        else:
            mat=upper if avg[2]>35 else lower;coords=[]
            arm=abs(avg[0]-cx)>8.4 or sum('arm' in base.bones[int(vb[i])]['name'].lower() for i in ids)>=2
            legleft=avg[0]<cx
            for i in ids:
                x,y,z=m.points[i];front=avg[1]<cy
                if mat==upper:
                    if arm:coords.append(tex(UP,'sleeve',(abs(x-cx)-7)/24,(58.5-z)/8))
                    else:coords.append(tex(UP,'front' if front else 'back',(x-cx+9)/18 if front else 1-(x-cx+9)/18,(59-z)/23))
                elif front:
                    side='R' if legleft else 'L';thigh=base.bind[names[f'Bip01 {side} Thigh']][:3,3];ankle=base.bind[names[f'Bip01 {side} Foot']][:3,3]
                    v=np.clip((z-ankle[2])/(thigh[2]-ankle[2]),0,1);center=ankle*(1-v)+thigh*v
                    coords.append(tex(LOW,'left' if legleft else 'right',(x-center[0]+4.4)/8.8,(37-z)/32))
                else:
                    side='R' if legleft else 'L';thigh=base.bind[names[f'Bip01 {side} Thigh']][:3,3];ankle=base.bind[names[f'Bip01 {side} Foot']][:3,3]
                    v=np.clip((z-ankle[2])/(thigh[2]-ankle[2]),0,1);center=ankle*(1-v)+thigh*v
                    coords.append(tex(LOW,'legback',(.5 if legleft else 0)+(1-(x-center[0]+4.4)/8.8)*.5,(37-z)/33))
            coords=tuple(coords)
        index=len(m.faces);m.faces.append((mat,ids,coords))
        if preserve:m.corner_data[index]=[(int(nb[c[1]]),norms[c[1]].copy()) for c in corners]
    smooth_cloth(m,protected,upper,lower)
    # An open hood surrounds the actual native head, never a synthetic face.
    headpoints=original[[i for i,b in enumerate(vb) if int(b) in heads]]
    lo,hi=headpoints.min(axis=0),headpoints.max(axis=0);center=(lo+hi)/2
    rx=max(4.1,(hi[0]-lo[0])/2+.80);rz=max(5.3,(hi[2]-lo[2])/2+1.0);ry=max(4.6,(hi[1]-lo[1])/2+1.05)
    m.material=upper;t.RECTS.update(UP);t.SWATCH.update(SWATCH_UP)
    rows=[];segments=16
    for q in np.linspace(.78,math.pi-.06,6):
        row=[]
        for j in range(segments):
            a=j*math.tau/segments;p=center+np.array([rx*math.sin(q)*math.cos(a),-ry*math.cos(q),rz*math.sin(q)*math.sin(a)])
            p[2]+=.45*max(0,math.sin(a))**4
            row.append(m.vertex(p,head,(p-center)/np.array([rx*rx,ry*ry,rz*rz])))
        rows.append(row)
    for i in range(len(rows)-1):
        for j in range(segments):
            k=(j+1)%segments;m.quad((rows[i][j],rows[i+1][j],rows[i+1][k],rows[i][k]),
              (tex(UP,'hood',j/segments,i/5),tex(UP,'hood',j/segments,(i+1)/5),tex(UP,'hood',(j+1)/segments,(i+1)/5),tex(UP,'hood',(j+1)/segments,i/5)))
    pole=m.vertex(center+np.array([0,ry,0]),head,[0,1,0])
    for j in range(segments):m.triangle(rows[-1][j],rows[-1][(j+1)%segments],pole,(tex(UP,'hood',j/segments,.98),tex(UP,'hood',(j+1)/segments,.98),tex(UP,'hood',.5,.99)))
    inner=[];rim=[]
    for j in range(segments):
        a=j*math.tau/segments;outer=m.points[rows[0][j]]
        p=center+np.array([rx*.64*math.cos(a),-ry*.72,rz*.65*math.sin(a)])
        rim.append(m.vertex((outer+p)*.5+np.array([0,-.20,0]),head,[0,-1,0]));inner.append(m.vertex(p,head,[0,-1,0]))
    for j in range(segments):
        k=(j+1)%segments
        m.quad((rows[0][j],rim[j],rim[k],rows[0][k]),(tex(UP,'hood',j/segments,.1),tex(UP,'hood',j/segments,.18),tex(UP,'hood',(j+1)/segments,.18),tex(UP,'hood',(j+1)/segments,.1)))
        m.quad((rim[j],inner[j],inner[k],rim[k]),(SWATCH_UP['dark'],)*4)
    # Rounded toe, shaped instep, ankle shaft and bevelled rubber sole. All
    # retain the existing foot bone and native weapon/animation behavior.
    m.material=shoe
    for side in ('L','R'):
        foot=names[f'Bip01 {side} Foot'];footset={i for i,b in enumerate(vb) if side.lower() in base.bones[int(b)]['name'].lower().split() and any(s in base.bones[int(b)]['name'].lower() for s in ('foot','toe','ankle'))}
        pts=original[list(footset)];lo,hi=pts.min(axis=0),pts.max(axis=0);center=(lo+hi)/2
        rx=(hi[0]-lo[0])/2+.18;ry=(hi[1]-lo[1])/2+.14;z=lo[2]-.05
        layers=[(z,1.02,1.0,0),(z+.45,1.07,1.02,0),(z+1.35,1.04,1.0,0),
                (z+2.9,1.,.90,.4),(z+5.3,.84,.65,2.4),(z+8.1,.78,.48,2.6),(z+10.3,.80,.47,2.6)]
        rings=[];segments=12
        for zi,sx,sy,dy in layers:
            row=[]
            for j in range(segments):
                a=j*math.tau/segments
                xx=math.copysign(abs(math.sin(a))**.78,math.sin(a))*rx*sx
                yy=-math.copysign(abs(math.cos(a))**.78,math.cos(a))*ry*sy
                row.append(m.vertex([center[0]+xx,center[1]+dy+yy,zi],foot,[xx,yy,0]))
            rings.append(row)
        for i in range(len(rings)-1):
            for j in range(segments):
                k=(j+1)%segments;ids=(rings[i][j],rings[i+1][j],rings[i+1][k],rings[i][k]);points=np.array([m.points[v] for v in ids]);avg=points.mean(axis=0)
                chart='sole' if i<2 else 'tongue' if i>=4 and avg[1]<center[1]+.4 else 'vamp' if avg[1]<center[1]-2.5 else 'outer'
                coords=[]
                for x,y,zz in points:
                    if chart=='sole':u=(y-lo[1])/(hi[1]-lo[1]);v=1-(zz-z)/1.35
                    elif chart=='vamp':u=(x-center[0]+rx)/(2*rx);v=1-(y-lo[1])/(ry*1.3)
                    elif chart=='tongue':u=(x-center[0]+rx*.85)/(rx*1.7);v=(z+10.3-zz)/6.5
                    else:u=(y-lo[1])/(hi[1]-lo[1]);v=(z+10.3-zz)/10.3
                    coords.append(tex(SHOE,chart,u,v))
                m.quad(ids,coords)
        bottom=m.vertex([center[0],center[1],z],foot,[0,0,-1])
        for j in range(segments):m.triangle(rings[0][j],bottom,rings[0][(j+1)%segments],(tex(SHOE,'sole',.5,.9),)*3)
    return m,{'native_vertices':source['verts'],'native_triangles':len(base.triangles(source)),'native_head_hand_triangles_preserved':kept,'native_head_hand_vertices_preserved':len(protected)}

def write(base,mesh,images,filename,destination):
    # Drop unused stock boot/gear vertices before native CPU skinning.
    used=sorted({v for _,ids,_ in mesh.faces for v in ids});remap={v:i for i,v in enumerate(used)}
    mesh.points=[mesh.points[i] for i in used];mesh.bones=[mesh.bones[i] for i in used]
    mesh.faces=[(mat,tuple(remap[v] for v in ids),uvs) for mat,ids,uvs in mesh.faces]
    data=bytearray(base.data)
    def align():
        while len(data)%4:data.append(0)
    def add(b):align();o=len(data);data.extend(b);return o
    pts=np.asarray(mesh.points);bones=np.asarray(mesh.bones,np.uint8);normal_sum=np.zeros_like(pts)
    for mat,ids,uvs in mesh.faces:
        a,b,c=pts[list(ids)];n=-np.cross(b-a,c-a)
        for v in ids:normal_sum[v]+=n
    materials=sorted({f[0] for f in mesh.faces});groups={k:[] for k in materials};index={};refs={}
    for fi,(mat,ids,uvs) in enumerate(mesh.faces):
        for ci,v in enumerate(ids):
            if fi in mesh.corner_data:bone,n=mesh.corner_data[fi][ci];local=t.unit(n)
            else:bone=int(bones[v]);local=t.unit(base.bind[bone][:3,:3].T@t.unit(normal_sum[v]))
            key=(mat,v,bone,tuple(np.round(local,7)))
            if key not in index:index[key]=(mat,len(groups[mat]));groups[mat].append((bone,local))
            refs[fi,ci]=key
    starts={};ordered=[]
    for mat in materials:starts[mat]=len(ordered);ordered+=groups[mat]
    assert len(pts)<=2048 and len(ordered)<=2048
    local=np.array([base.bind[int(b)][:3,:3].T@(p-base.bind[int(b)][:3,3]) for p,b in zip(pts,bones)],'<f4')
    vb=add(bones.tobytes());nb=add(bytes(n[0] for n in ordered));vp=add(local.tobytes());np_=add(np.array([n[1] for n in ordered],'<f4').tobytes());meshes=[]
    submission={'commands':0,'submitted_corners':0}
    for mat in materials:
        triangles=[]
        for fi,(material,ids,coords) in enumerate(mesh.faces):
            if material!=mat:continue
            corners=[]
            for ci,(v,uv_) in enumerate(zip(ids,coords)):
                material,index_in_group=index[refs[fi,ci]];ni=starts[material]+index_in_group
                corners.append((v,ni,*uv_))
            triangles.append(tuple(corners))
        commands,stats=studio_strips.pack(triangles)
        for key in submission:submission[key]+=stats[key]
        at=add(commands);meshes.append(struct.pack('<5i',len(triangles),at,mat,len(groups[mat]),0))
    mi=add(b''.join(meshes));model=add(struct.pack('<64sif10i',filename.encode(),0,72.,len(meshes),mi,len(pts),vb,vp,len(ordered),nb,np_,0,0))
    body=bytearray(base.data[base.h['bodypartindex']:base.h['bodypartindex']+76*base.h['numbodyparts']]);struct.pack_into('<i',body,72,model);bodyat=add(body)
    oldcount=base.h['numtextures'];table=bytearray(base.data[base.h['textureindex']:base.h['textureindex']+80*oldcount])
    for j,im in enumerate(images):table+=struct.pack('<64s4i',(filename+['_upper','_lower','_shoes'][j]+'.bmp').encode(),0,im.width,im.height,0)
    tableat=add(bytes(len(table)));skin=np.frombuffer(base.data,'<i2',base.h['numskinref']*base.h['numskinfamilies'],base.h['skinindex']).reshape(base.h['numskinfamilies'],-1)
    skinat=add(np.column_stack([skin]+[np.full(len(skin),oldcount+j) for j in range(len(images))]).astype('<i2').tobytes());align();boundary=len(data)
    for j in range(oldcount):
        flags,w,h,at=struct.unpack_from('<4i',base.data,base.h['textureindex']+80*j+64);assert w*h<=640*480
        offset=add(base.data[at:at+w*h+768]);struct.pack_into('<i',table,j*80+76,offset)
    for j,im in enumerate(images):
        assert im.width==512 and im.height==512;offset=add(im.tobytes()+(bytes(im.getpalette()[:768])+bytes(768))[:768]);struct.pack_into('<i',table,(oldcount+j)*80+76,offset)
    data[tableat:tableat+len(table)]=table
    for key,value in [('numtextures',oldcount+len(images)),('textureindex',tableat),('texturedataindex',boundary),('numskinref',base.h['numskinref']+len(images)),('skinindex',skinat),('bodypartindex',bodyat)]:struct.pack_into('<i',data,136+4*t.HEADER_NAMES.index(key),value)
    struct.pack_into('<i',data,72,len(data));data[8:72]=(f'models/baloncuaga_outfits/{filename}.mdl'.encode()+bytes(64))[:64]
    assert data[244:len(base.data)]==base.data[244:]
    destination.parent.mkdir(parents=True,exist_ok=True);destination.write_bytes(data)
    return {'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),'bones':base.h['numbones'],'sequences':base.h['numseq'],'vertices':len(pts),'normals':len(ordered),'triangles':len(mesh.faces),**submission,'cpu_boundary':boundary,'texture_sizes':[[im.width,im.height] for im in images]}

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--players',required=True,type=Path);ap.add_argument('--output',type=Path,default=Path(__file__).resolve().parents[1]/'cstrike/models/baloncuaga_outfits');ap.add_argument('--previews',type=Path,default=Path(__file__).resolve().parents[1]/'previews_native');ap.add_argument('--no-previews',action='store_true');args=ap.parse_args();reports={}
    for native,team,filename in VARIANTS:
        base=t.Mdl(args.players/native/(native+'.mdl'));images=textures(team);mesh,kept=create(base,team);path=args.output/(filename+'.mdl');record=write(base,mesh,images,filename,path)
        reports[filename]={**record,**kept,'native':native,'team':team,'path':'cstrike/models/baloncuaga_outfits/'+filename+'.mdl','source_bytes':len(base.data),'source_textureindex':base.h['textureindex'],'source_numtextures':base.h['numtextures']};print(filename,json.dumps(reports[filename]),flush=True)
        if not args.no_previews:
            final=t.Mdl(path)
            for suffix,angle in [('front',0),('back',180)]:t.render(final,args.previews/(filename+'_'+suffix+'.png'),None,0,angle,native.upper()+' / native mesh + outfit')
    args.output.joinpath('native_manifest.json').write_text(json.dumps({'models':reports},indent=2)+'\n','utf-8')
if __name__=='__main__':main()
