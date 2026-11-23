"""Build original low-poly streetwear on supplied CS 1.6 CT/TT MDLs.

Animation, skeleton, attachment, hitbox and event bytes remain at their original
offsets. The renderer below reads the finished MDL, not an authoring scene.
Requires Python3, NumPy and Pillow. No reference photographs are embedded.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import struct
import numpy as np
from PIL import Image,ImageDraw,ImageFont,ImageFilter

TAU=math.tau
HEADER_NAMES=['flags','numbones','boneindex','numbonecontrollers','bonecontrollerindex','numhitboxes','hitboxindex','numseq','seqindex','numseqgroups','seqgroupindex','numtextures','textureindex','texturedataindex','numskinref','numskinfamilies','skinindex','numbodyparts','bodypartindex','numattachments','attachmentindex']
RECTS={'front':(0,0,384,384),'back':(384,0,384,384),'sleeve':(768,0,256,384),
       'left':(0,384,256,384),'right':(256,384,256,384),'legback':(512,384,256,384),
       'hood':(768,384,256,256),'boot':(0,768,512,256),'sole':(512,768,256,256),'face':(768,768,256,256)}
SWATCH={'black':(784,656),'silver':(816,656),'cream':(848,656),'red':(880,656),'dark':(912,656),'stitch':(944,656),'skin':(976,656)}

def name(raw):return raw.split(b'\0')[0].decode('ascii','replace')
def unit(v):
    v=np.asarray(v,float);length=np.linalg.norm(v)
    return v/length if length>1e-10 else np.array([0.,0.,1.])
def transform(angles,pos):
    x,y,z=angles;cx,sx=math.cos(x),math.sin(x);cy,sy=math.cos(y),math.sin(y);cz,sz=math.cos(z),math.sin(z)
    m=np.eye(4);m[:3,:3]=[[cz*cy,cz*sy*sx-sz*cx,cz*sy*cx+sz*sx],[sz*cy,sz*sy*sx+cz*cx,sz*sy*cx-cz*sx],[-sy,cy*sx,cy*cx]];m[:3,3]=pos;return m

class Mdl:
    def __init__(self,path):
        self.path=Path(path);self.data=self.path.read_bytes();self.h={n:self.i(136+4*j) for j,n in enumerate(HEADER_NAMES)}
        if self.data[:4]!=b'IDST' or self.i(4)!=10 or self.i(72)!=len(self.data):raise ValueError('Expected a complete GoldSrc MDL version10.')
        if self.h['numbones'] not in (53,55) or self.h['numseq']!=111 or self.h['numseqgroups']!=1:raise ValueError('Expected stock CS1.6 urban skeleton and111 internal sequences.')
        self.bones=[]
        for j in range(self.h['numbones']):
            o=self.h['boneindex']+112*j
            self.bones.append({'name':name(self.data[o:o+32]),'parent':self.i(o+32),'values':np.array(struct.unpack_from('<6f',self.data,o+64)),'scales':np.array(struct.unpack_from('<6f',self.data,o+88))})
        self.bind=self.pose()
    def i(self,o):return struct.unpack_from('<i',self.data,o)[0]
    def animation_value(self,at,frame):
        loops=0
        while True:
            valid,total=struct.unpack_from('<BB',self.data,at)
            if not total or valid>total:raise ValueError('Invalid animation stream.')
            if frame<total:return struct.unpack_from('<h',self.data,at+2*(min(frame,valid-1)+1))[0]
            frame-=total;at+=2*(valid+1);loops+=1
            if loops>10000:raise ValueError('Unbounded animation stream.')
    def pose(self,sequence=None,frame=0,blend=0):
        anim=None
        if sequence is not None:
            seq=self.h['seqindex']+176*sequence;frames=self.i(seq+56);frame=min(max(0,int(frame)),max(0,frames-1));blends=self.i(seq+120)
            if self.i(seq+156)!=0:raise ValueError('External animation groups are unsupported.')
            anim=self.i(seq+124)+min(blend,max(0,blends-1))*self.h['numbones']*12
        matrices=[]
        for j,b in enumerate(self.bones):
            values=b['values'].copy()
            if anim is not None:
                at=anim+12*j
                for axis,offset in enumerate(struct.unpack_from('<6H',self.data,at)):
                    if offset:values[axis]+=self.animation_value(at+offset,frame)*b['scales'][axis]
            m=transform(values[3:],values[:3]);matrices.append(m if b['parent']<0 else matrices[b['parent']]@m)
        return matrices
    def texture(self,index):
        o=self.h['textureindex']+80*index;flags,w,h,at=struct.unpack_from('<4i',self.data,o+64)
        pix=np.frombuffer(self.data,np.uint8,w*h,at).reshape(h,w);pal=np.frombuffer(self.data,np.uint8,768,at+w*h).reshape(256,3)
        return pal[pix].copy()
    def body_model(self,index=0,variant=0):
        at=self.h['bodypartindex']+76*index;count,base,models=struct.unpack_from('<3i',self.data,at+64);o=models+min(variant,count-1)*112
        values=struct.unpack_from('<i f 10i',self.data,o+64)
        return {'at':o,'meshes':values[2],'meshindex':values[3],'verts':values[4],'vertbone':values[5],'vertindex':values[6],'norms':values[7],'normbone':values[8],'normindex':values[9]}
    def triangles(self,model):
        result=[]
        for mesh in range(model['meshes']):
            n,at,skin,norms,normindex=struct.unpack_from('<5i',self.data,model['meshindex']+20*mesh)
            found=0
            while True:
                command=struct.unpack_from('<h',self.data,at)[0];at+=2
                if not command:break
                count=abs(command);corners=[struct.unpack_from('<4h',self.data,at+8*k) for k in range(count)];at+=8*count
                for k in range(2,count):
                    order=(0,k-1,k) if command<0 else (k-2,k-1,k) if k%2==0 else (k-1,k-2,k)
                    result.append((skin,[corners[p] for p in order]));found+=1
            if found!=n:raise ValueError('Triangle command count does not match mesh header.')
        return result
    def geometry(self,sequence=None,frame=0):
        model=self.body_model();pose=self.pose(sequence,frame);verts=np.frombuffer(self.data,'<f4',model['verts']*3,model['vertindex']).reshape(-1,3)
        bones=np.frombuffer(self.data,np.uint8,model['verts'],model['vertbone']);world=np.array([pose[int(b)][:3,:3]@v+pose[int(b)][:3,3] for v,b in zip(verts,bones)])
        norms=np.frombuffer(self.data,'<f4',model['norms']*3,model['normindex']).reshape(-1,3);nb=np.frombuffer(self.data,np.uint8,model['norms'],model['normbone']);normals=np.array([pose[int(b)][:3,:3]@v for v,b in zip(norms,nb)])
        skin=np.frombuffer(self.data,'<i2',self.h['numskinref'],self.h['skinindex'])
        return world,normals,self.triangles(model),skin

def font(file,size):
    windows=os.environ.get('WINDIR') or os.environ.get('SYSTEMROOT')
    folder=Path(windows)/'Fonts' if windows else Path('Fonts')
    try:return ImageFont.truetype(str(folder/file),size)
    except OSError:return ImageFont.load_default()
def fit_text(draw,text,box,file='timesbd.ttf',size=66,fill=(222,218,205),outline=0):
    x,y,w,h=box;face=font(file,size)
    while size>8:
        b=draw.textbbox((0,0),text,font=face,stroke_width=outline)
        if b[2]-b[0]<=w and b[3]-b[1]<=h:break
        size-=1;face=font(file,size)
    px=x+(w-(b[2]-b[0]))/2-b[0];py=y+(h-(b[3]-b[1]))/2-b[1]
    draw.text((px,py),text,font=face,fill=fill,stroke_width=outline,stroke_fill=fill)
    return (px,py),face

def make_texture(team):
    rng=np.random.default_rng(17237 if team=='ct' else 82719);y,x=np.mgrid[:1024,:1024]
    base=31 if team=='ct' else 45
    noise=rng.normal(0,1.8,(1024,1024));fold=3*np.sin(x*.047+y*.016)+2*np.sin(x*.118+y*.007)+1.5*np.sin(y*.23)
    cloth=np.clip(base+noise+fold,12,68);rgb=np.stack([cloth,cloth+.5,cloth+1.5],axis=-1).astype(np.uint8)
    im=Image.fromarray(rgb,'RGB');d=ImageDraw.Draw(im)
    for key in ('front','back','sleeve','left','right','legback','hood'):
        a,b,w,h=RECTS[key]
        for p in range(9):
            px=a+int((p+.5)*w/9);curve=[(px+math.sin(t*.8+p)*6,b+t*h/14) for t in range(15)]
            d.line(curve,fill=(max(18,base-7),)*3,width=2)
            d.line([(xx+2,yy) for xx,yy in curve],fill=(base+7,)*3,width=1)
    # Ribbed hem, shoulder seams and a stitched kangaroo pocket.
    for a,b,w,h in (RECTS['front'],RECTS['back']):
        d.line((a+8,b+366,a+w-8,b+366),fill=(68,68,68),width=2)
        for xx in range(a+10,a+w-10,5):d.line((xx,b+368,xx,b+383),fill=(21,21,22),width=1)
    d.line([(62,274),(83,245),(300,245),(325,274),(310,350),(78,350),(62,274)],fill=(77,77,78),width=2)
    d.line([(66,278),(86,249),(297,249),(320,278)],fill=(21,21,22),width=4)
    if team=='ct':
        lettering,face=fit_text(d,'VETEMENTS',(16,69,352,82),size=62,outline=2)
        # Hollow collegiate lettering: white outer stroke and a dark core.
        d.text(lettering,'VETEMENTS',font=face,fill=(38,38,40),stroke_width=0)
        fit_text(d,'FAN?',(58,163,268,88),size=88)
        for yy in range(4,382,7):d.rectangle((190,yy,193,yy+3),fill=(151,151,151))
        d.line((188,0,188,383),fill=(15,15,15),width=2);d.line((196,0,196,383),fill=(84,84,84),width=1)
        fit_text(d,'ATLANTA',(524,396,232,60),'arialbd.ttf',size=44,fill=(174,28,39))
        fit_text(d,'6',(65,431,120,120),'arial.ttf',size=124,fill=(174,28,39))
        fit_text(d,'LOTTA',(291,493,188,43),'arialbd.ttf',size=42,fill=(180,29,39));fit_text(d,'mowa',(300,556,168,37),'arialbd.ttf',size=32)
        # Original horse-and-rider silhouette, intentionally simplified.
        horse=(226,224,214);d.ellipse((336,424,397,450),fill=horse);d.polygon([(387,434),(400,411),(415,418),(403,443)],fill=horse);d.ellipse((402,408,421,423),fill=horse)
        for xx in (343,362,383,393):d.line((xx,444,xx-6,474),fill=horse,width=5)
        d.ellipse((359,396,371,409),fill=horse);d.line((365,407,365,432),fill=horse,width=6);d.line((365,421,397,426),fill=horse,width=4)
    else:
        fit_text(d,'ENFANTS RICHES',(38,103,308,52),size=47)
        fit_text(d,'DÉPRIMÉS',(76,157,232,50),size=47)
        # Wash abrasion stays subtle, with hand-placed frayed pocket edges.
        for _ in range(4000):
            xx=int(rng.integers(40,344));yy=int(rng.integers(90,214))
            if rng.random()<.55:d.point((xx,yy),fill=(46,47,48))
        for a,b,w,h in (RECTS['left'],RECTS['right']):
            points=[(a+int(w*(.72-.20*t/18+.09*math.sin(t*.21))),b+t*h/18) for t in range(19)]
            d.line(points,fill=(164,160,148),width=4)
            for xx,yy in points:d.line((xx-5,yy,xx+5,yy+2),fill=(83,82,79),width=2)
        for xx in (44,218,300,478):d.line((xx,400,xx+4,449),fill=(91,91,89),width=2)
    # Hood seams/lining and shoe leather texture.
    d.line((780,413,1008,413),fill=(83,83,84),width=3);d.line((894,388,894,634),fill=(27,27,29),width=3)
    boot=np.asarray(im)[768:1024,:512].copy();by,bx=np.mgrid[:256,:512];leather=np.clip(28+rng.normal(0,2.0,(256,512))+4*np.sin(bx*.028),14,51).astype(np.uint8);boot[:]=leather[...,None];im.paste(Image.fromarray(boot),(0,768));d=ImageDraw.Draw(im)
    for yy in (786,807,838,882,940,997):d.line((10,yy,500,yy),fill=(61,61,62),width=2)
    cream=(218,207,182) if team=='tt' else (29,29,30);d.rectangle((512,768,767,1023),fill=cream)
    for yy in range(778,1020,11):d.line((520,yy,760,yy),fill=tuple(max(0,c-18) for c in cream),width=1)
    # A newly drawn face atlas; hood shadows cover its outer edges.
    fy,fx=np.mgrid[:256,:256];shade=np.maximum(0,1-((fx-128)/145)**2)*np.maximum(.2,1-((fy-135)/220)**2)
    skin=np.stack([55+74*shade,47+61*shade,43+49*shade],axis=-1).astype(np.uint8);im.paste(Image.fromarray(skin),(768,768));d=ImageDraw.Draw(im)
    for ex in (848,943):
        d.line((ex-18,864,ex+17,861),fill=(36,31,28),width=7);d.ellipse((ex-12,870,ex+13,877),fill=(35,31,29));d.ellipse((ex-4,870,ex+1,876),fill=(104,100,89))
    d.line((897,873,891,911,904,914),fill=(76,61,50),width=4);d.line((869,937,923,936),fill=(68,48,43),width=3)
    for key,point in SWATCH.items():
        color={'black':(31,31,32),'silver':(145,145,140),'cream':(218,207,182),'red':(168,26,38),'dark':(16,16,17),'stitch':(76,76,76),'skin':(123,103,85)}[key];xx,yy=point;d.rectangle((xx-12,yy-12,xx+12,yy+12),fill=color)
    # Quantization is required by the original 8-bit Studio texture format.
    return im.quantize(colors=256,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE)

def uv(rect,u,v):
    x,y,w,h=RECTS[rect] if isinstance(rect,str) else rect
    return (int(round(x+2+min(1,max(0,u))*(w-5))),int(round(y+2+min(1,max(0,v))*(h-5))))

class Mesh:
    def __init__(self,base):self.base=base;self.points=[];self.bones=[];self.faces=[];self.hints=[];self.material=base.h["numskinref"]
    def vertex(self,p,bone,hint=None):self.points.append(np.array(p,float));self.bones.append(int(bone));self.hints.append(None if hint is None else unit(hint));return len(self.points)-1
    def triangle(self,a,b,c,coords,material=None):
        hints=[self.hints[i] for i in (a,b,c) if self.hints[i] is not None]
        # GoldSrc Studio uses clockwise exterior triangles (GL_FRONT culling).
        if hints and np.dot(np.cross(self.points[b]-self.points[a],self.points[c]-self.points[a]),np.mean(hints,axis=0))>0:
            b,c=c,b;coords=(coords[0],coords[2],coords[1])
        self.faces.append((self.material if material is None else material,(a,b,c),tuple(coords)))
    def quad(self,indices,coords,material=None):
        a,b,c,d=indices;u,v,w,x=coords;self.triangle(a,b,c,(u,v,w),material);self.triangle(a,c,d,(u,w,x),material)
    def flat(self,points,bone,swatch,outside=None):
        if outside is None:outside=np.cross(np.asarray(points[1])-points[0],np.asarray(points[2])-points[0])
        ids=[self.vertex(p,bone,outside) for p in points];st=SWATCH[swatch]
        for j in range(1,len(ids)-1):self.triangle(ids[0],ids[j],ids[j+1],(st,st,st))
    def ribbon(self,path,bones,width,swatch):
        rows=[]
        for i,p in enumerate(path):
            tangent=unit(np.asarray(path[min(len(path)-1,i+1)])-np.asarray(path[max(0,i-1)]));across=unit(np.cross(tangent,[0,-1,0]))*width*.5
            rows.append([self.vertex(np.asarray(p)-across,bones[i],[0,-1,0]),self.vertex(np.asarray(p)+across,bones[i],[0,-1,0])])
        st=SWATCH[swatch]
        for a,b in zip(rows,rows[1:]):self.quad((a[0],a[1],b[1],b[0]),(st,st,st,st))
    def tube(self,rings,segments,chart='hood',side_charts=None,closed=False,phase=-math.pi/2,shape=2,flipv=False):
        # Rings: center, horizontal radius, depth radius, bone. Axis is world Z
        # for torso/legs and custom ring frames for arms.
        indices=[];count=len(rings)
        for i,ring in enumerate(rings):
            center,rx,ry,bone=ring[:4];frame=ring[4] if len(ring)>4 else np.array([[1,0,0],[0,1,0]])
            row=[]
            for j in range(segments):
                angle=phase+j*TAU/segments;xx=math.copysign(abs(math.sin(angle))**(2/shape),math.sin(angle))*rx;yy=-math.copysign(abs(math.cos(angle))**(2/shape),math.cos(angle))*ry
                radial=frame[0]*xx+frame[1]*yy;row.append(self.vertex(np.asarray(center)+radial,bone,radial))
            indices.append(row)
        for i in range(count-1):
            for j in range(segments):
                k=(j+1)%segments;angle=phase+(j+.5)*TAU/segments;front=math.cos(angle)>0
                rect=side_charts[0 if front else 1] if side_charts else chart
                def tex(q,r):
                    phi=phase+q*TAU/segments
                    u=(math.sin(phi)+1)/2 if side_charts else q/segments
                    if not front and side_charts:u=1-u
                    return uv(rect,u,1-r/(count-1) if flipv else r/(count-1))
                self.quad((indices[i][j],indices[i+1][j],indices[i+1][k],indices[i][k]),(tex(j,i),tex(j,i+1),tex(j+1,i+1),tex(j+1,i)))
        if closed:
            for row,reverse in ((indices[0],True),(indices[-1],False)):
                for j in range(1,segments-1):
                    ids=(row[0],row[j+1],row[j]) if reverse else (row[0],row[j],row[j+1]);self.triangle(*ids,(uv(chart,.5,.5),)*3)
        return indices


def render(model,path,sequence=None,frame=0,angle=0,title=''):
    world,normals,triangles,skin=model.geometry(sequence,frame);textures=[model.texture(i) for i in range(model.h['numtextures'])]
    if sequence is not None:
        posed=model.pose(sequence,frame);facing=posed[5][:3,:3]@model.bind[5][:3,:3].T@np.array([0.,-1.,0.]);angle+=math.degrees(math.atan2(facing[0],-facing[1]))
    theta=math.radians(angle);forward=np.array([math.sin(theta),-math.cos(theta),.10]);forward=unit(forward);right=unit(np.cross([0,0,1],forward));up=unit(np.cross(forward,right))
    W,H=900,900;raw=np.stack((world@right,-world@up,world@forward),axis=-1);extent=np.ptp(raw,axis=0);scale=min(10.5,(W-150)/max(1,extent[0]),(H-150)/max(1,extent[1]));center=(raw.min(axis=0)+raw.max(axis=0))/2
    project=raw.copy();project[:,:2]=(raw[:,:2]-center[:2])*scale+np.array([W/2,H/2+28])
    yy,xx=np.mgrid[:H,:W];bg=(65+13*(1-yy/H)+7*np.exp(-((xx-W*.5)/(W*.43))**2));image=np.stack((bg*.98,bg,bg*1.03),axis=-1).astype(np.float32);depth=np.full((H,W),-1e10)
    light=unit(forward*.9+np.array([-.3,-.2,.85]));lighting=np.clip(.56+np.maximum(0,normals@light)*.69+np.maximum(0,normals@unit((1,.7,.3)))*.20,.44,1.22)
    for material,corners in triangles:
        ids=[c[0] for c in corners];positions=world[ids]
        if np.dot(-np.cross(positions[1]-positions[0],positions[2]-positions[0]),forward)<=0:continue
        pts=project[ids];uvs=np.array([[c[2],c[3]] for c in corners],float);lit=np.array([lighting[c[1]] for c in corners])
        xmin=max(0,int(math.floor(pts[:,0].min())));xmax=min(W-1,int(math.ceil(pts[:,0].max())));ymin=max(0,int(math.floor(pts[:,1].min())));ymax=min(H-1,int(math.ceil(pts[:,1].max())))
        if xmin>xmax or ymin>ymax:continue
        a,b,c=pts;den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
        if abs(den)<1e-8:continue
        gy,gx=np.mgrid[ymin:ymax+1,xmin:xmax+1];gx=gx+.5;gy=gy+.5
        wa=((b[1]-c[1])*(gx-c[0])+(c[0]-b[0])*(gy-c[1]))/den;wb=((c[1]-a[1])*(gx-c[0])+(a[0]-c[0])*(gy-c[1]))/den;wc=1-wa-wb;z=wa*a[2]+wb*b[2]+wc*c[2]
        target=depth[ymin:ymax+1,xmin:xmax+1];mask=(wa>=-1e-5)&(wb>=-1e-5)&(wc>=-1e-5)&(z>target)
        if not np.any(mask):continue
        tex=textures[int(skin[material])];s=wa*uvs[0,0]+wb*uvs[1,0]+wc*uvs[2,0];t=wa*uvs[0,1]+wb*uvs[1,1]+wc*uvs[2,1];color=tex[np.clip(t.astype(int),0,tex.shape[0]-1),np.clip(s.astype(int),0,tex.shape[1]-1)]
        shade=wa*lit[0]+wb*lit[1]+wc*lit[2];image[ymin:ymax+1,xmin:xmax+1][mask]=np.clip(color*shade[...,None],0,255)[mask];target[mask]=z[mask]
    result=Image.fromarray(np.clip(image,0,255).astype(np.uint8));draw=ImageDraw.Draw(result);draw.text((24,22),title,font=font('arialbd.ttf',24),fill=(210,213,218));draw.text((24,55),'Actual generated MDL mesh / native CS animation',font=font('arial.ttf',14),fill=(126,133,145));path.parent.mkdir(parents=True,exist_ok=True);result.save(path)

