"""Lossless GoldSrc strip/fan packing of fully attributed triangle corners.

Normal and UV seams are part of each corner key. Only cyclic rotations are
equivalent; reversed winding or stitching across a texture seam is forbidden.
"""
from collections import Counter,defaultdict
import struct

def canonical(tri):
    return min(tuple(tri[i:]+tri[:i]) for i in range(3))

def decode(commands):
    faces=[]
    for fan,points in commands:
        for i in range(2,len(points)):
            order=(0,i-1,i) if fan else (i-2,i-1,i) if i%2==0 else (i-1,i-2,i)
            faces.append(tuple(points[j] for j in order))
    return faces

def pack(triangles,max_corners=128):
    triangles=[tuple(tuple(c) for c in tri) for tri in triangles]
    edges=defaultdict(list)
    for i,tri in enumerate(triangles):
        for j in range(3):edges[tri[j],tri[(j+1)%3]].append((i,tri[(j+2)%3]))
    unused=set(range(len(triangles)));commands=[]
    def grow(seed,rotation,fan):
        tri=triangles[seed];points=list(tri[rotation:]+tri[:rotation]);used={seed}
        while len(points)<max_corners:
            edge=(points[0],points[-1]) if fan else (points[-2],points[-1]) if len(points)%2==0 else (points[-1],points[-2])
            found=None
            for index,corner in edges.get(edge,()):
                if index in unused and index not in used:found=(index,corner);break
            if found is None:break
            used.add(found[0]);points.append(found[1])
        return fan,points,used
    while unused:
        seed=min(unused)
        choices=[grow(seed,rotation,fan) for fan in (False,True) for rotation in range(3)]
        fan,points,used=max(choices,key=lambda c:len(c[2]))
        commands.append((fan,points));unused-=used
    assert Counter(map(canonical,decode(commands)))==Counter(map(canonical,triangles)), 'Triangle attributes/winding changed'
    raw=bytearray()
    for fan,points in commands:
        raw+=struct.pack('<h',-len(points) if fan else len(points))
        for corner in points:raw+=struct.pack('<4h',*corner)
    raw+=b'\0\0'
    return raw,{'commands':len(commands),'submitted_corners':sum(len(p) for _,p in commands)}
