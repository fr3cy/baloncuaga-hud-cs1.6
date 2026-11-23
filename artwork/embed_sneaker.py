"""Convert the branding PNG into a padded, embedded RLE TGA32 resource.

This is a technical packaging step: artwork, colours and text are unchanged.
Requires Pillow. Run from any directory; all defaults are relative to this file.
"""
import argparse
from pathlib import Path
import struct
from PIL import Image

def package(source,header,preview):
    im=Image.open(source).convert('RGBA')
    bounds=im.getchannel('A').getbbox()
    if not bounds:
        raise ValueError('The shoe artwork is fully transparent.')
    im=im.crop(bounds)
    im.thumbnail((480,224),Image.Resampling.LANCZOS)
    canvas=Image.new('RGBA',(512,256),(0,0,0,0))
    canvas.alpha_composite(im,((512-im.width)//2,(256-im.height)//2))
    # TGA32, top-first rows, eight alpha bits. Packet lengths are at most128.
    raw=canvas.tobytes('raw','BGRA')
    pixels=[raw[i:i+4] for i in range(0,len(raw),4)]
    body=bytearray();at=0
    while at<len(pixels):
        end=at+1
        while end<min(at+128,len(pixels)) and pixels[end]==pixels[at]:
            end+=1
        if end-at>=3:
            body.append(128+end-at-1);body.extend(pixels[at]);at=end
        else:
            end=at+1
            while end<min(at+128,len(pixels)):
                if end+2<len(pixels) and pixels[end]==pixels[end+1]==pixels[end+2]:
                    break
                end+=1
            body.append(end-at-1);body.extend(b''.join(pixels[at:end]));at=end
    data=struct.pack('<BBBHHBHHHHBB',0,0,10,0,0,0,0,0,512,256,32,0x28)+body
    header.parent.mkdir(parents=True,exist_ok=True)
    lines=['#pragma once','// Generated from assets/branding/baloncuaga_3xl_cutout.png by artwork/embed_sneaker.py.',
           '// RLE TGA32; 512 x256, visual top first, straight RGBA after decoding.',
           'namespace SneakerImage {','static constexpr unsigned char tga[]={']
    lines += ['    '+','.join(f'0x{b:02x}' for b in data[i:i+16])+',' for i in range(0,len(data),16)]
    lines += ['};','} // namespace SneakerImage','']
    header.write_text('\n'.join(lines),encoding='utf-8')
    preview.parent.mkdir(parents=True,exist_ok=True);canvas.save(preview)
    print(f'Embedded shoe: {len(data)} TGA bytes; transparent padding verified.')

if __name__=='__main__':
    root=Path(__file__).resolve().parent.parent
    p=argparse.ArgumentParser()
    p.add_argument('--input',type=Path,default=root/'assets/branding/baloncuaga_3xl_cutout.png')
    p.add_argument('--header',type=Path,default=root/'source/sneaker_image.h')
    p.add_argument('--preview',type=Path,default=root/'assets/branding/baloncuaga_3xl_embedded.png')
    args=p.parse_args();package(args.input,args.header,args.preview)
