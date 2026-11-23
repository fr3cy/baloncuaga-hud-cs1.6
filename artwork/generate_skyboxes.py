"""Original, direction-continuous GoldSrc sky art. Code MIT; generated art CC0.

Procedural skies; the night emblem uses the included image_gen shoe asset.
Faces use the GoldSrc/Quake sky convention with decoded
row 0 at the visual top. TGA files are uncompressed BGR24, bottom-left origin.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import time

import numpy as np
from PIL import Image, ImageDraw

np.seterr(over='ignore', under='ignore', invalid='raise')
PI = np.float32(math.pi)
FACES = {
    'rt': ((1, 0, 0), (0, -1, 0), (0, 0, 1)),
    'bk': ((0, 1, 0), (1, 0, 0), (0, 0, 1)),
    'lf': ((-1, 0, 0), (0, 1, 0), (0, 0, 1)),
    'ft': ((0, -1, 0), (-1, 0, 0), (0, 0, 1)),
    'up': ((0, 0, 1), (0, -1, 0), (-1, 0, 0)),
    'dn': ((0, 0, -1), (0, -1, 0), (1, 0, 0)),
}
CATALOG = [
    ('sunny', 'Солнечное небо', 'Объёмные белые облака и тёплое солнце.', 14017),
    ('night', '3XL в небе', 'Светящийся baLONCUaga 3XL, серебристый ореол и звёзды.', 27011),
    ('sunset', 'Закат', 'Янтарный горизонт и освещённые края облаков.', 39019),
    ('overcast', 'Пасмурное небо', 'Многослойные серебристые грозовые облака.', 48023),
    ('aurora', 'Северное сияние', 'Изумрудные и фиолетовые световые ленты.', 61001),
    ('space', 'Космос', 'Галактическая полоса, туманности и звёздное поле.', 73009),
]


def smooth(a, b, x):
    f = np.clip((x-a)/(b-a), 0, 1)
    return f*f*(3-2*f)


def linear(rgb):
    return np.asarray(rgb, np.float32)**2.2


def unit(value):
    value = np.asarray(value, np.float32)
    return value / np.sqrt(np.sum(value*value, axis=-1, keepdims=True))


def celestial(azimuth, elevation):
    return unit((math.cos(elevation)*math.cos(azimuth), math.cos(elevation)*math.sin(azimuth), math.sin(elevation)))


def face_directions(face, s, t):
    normal, right, up = (np.asarray(v, np.float32) for v in FACES[face])
    return unit(normal+s[..., None]*right+t[..., None]*up)


def lattice(x, y, z, seed):
    h = x.astype(np.uint32)*np.uint32(0x8da6b343)
    h ^= y.astype(np.uint32)*np.uint32(0xd8163841)
    h ^= z.astype(np.uint32)*np.uint32(0xcb1ab31f)
    h ^= np.uint32(seed)
    h = (h ^ (h >> 16))*np.uint32(0x7feb352d)
    h = (h ^ (h >> 15))*np.uint32(0x846ca68b)
    return ((h ^ (h >> 16)).astype(np.float32)/np.float32(4294967295.0))


def noise(x, y, z, seed):
    ix, iy, iz = np.floor(x).astype(np.int32), np.floor(y).astype(np.int32), np.floor(z).astype(np.int32)
    u, v, w = x-ix, y-iy, z-iz
    u, v, w = (a*a*a*(a*(a*6-15)+10) for a in (u, v, w))
    a = lattice(ix, iy, iz, seed)*(1-u)+lattice(ix+1, iy, iz, seed)*u
    b = lattice(ix, iy+1, iz, seed)*(1-u)+lattice(ix+1, iy+1, iz, seed)*u
    c = lattice(ix, iy, iz+1, seed)*(1-u)+lattice(ix+1, iy, iz+1, seed)*u
    d = lattice(ix, iy+1, iz+1, seed)*(1-u)+lattice(ix+1, iy+1, iz+1, seed)*u
    return ((a*(1-v)+b*v)*(1-w)+(c*(1-v)+d*v)*w).astype(np.float32)


def fbm(x, y, z, seed, octaves=5):
    result = np.zeros_like(x, np.float32)
    weight, total = 1.0, 0.0
    for octave in range(octaves):
        result += noise(x, y, z, seed+octave*1877)*weight
        total += weight
        x, y, z = x*2.03+7.17, y*2.03-3.91, z*2.03+5.33
        weight *= .53
    return result/total


_EMBLEM = None

def shoe_in_sky(d, rgb):
    global _EMBLEM
    if _EMBLEM is None:
        path = Path(__file__).resolve().parents[1]/'assets'/'branding'/'baloncuaga_3xl_luminous.png'
        _EMBLEM = np.asarray(Image.open(path).convert('RGBA'), np.float32)/255
    image = _EMBLEM
    # Camera-right basis, matching native MakeSkyVec face orientation. Text on
    # the shoe stays readable from the player, rather than being mirrored.
    center = celestial(1.27, .50)
    right = unit(np.cross(center, np.array((0, 0, 1), np.float32)))
    up = np.cross(right, center)
    depth = np.sum(d*center, axis=-1)
    denominator = np.maximum(depth, .01)
    sx = np.sum(d*right, axis=-1)/denominator
    sy = np.sum(d*up, axis=-1)/denominator
    half_w = .43
    half_h = half_w*image.shape[0]/image.shape[1]
    # This is a distant luminous sky graphic, not a dynamic world-light source.
    halo = np.exp(-(sx/.68)**2-(sy/.38)**2)*.10 + np.exp(-(sx/.45)**2-(sy/.22)**2)*.16
    rgb += linear((.61, .73, 1.0))*(halo*(depth>0))[..., None]
    u, v = .5+sx/(2*half_w), .5-sy/(2*half_h)
    mask = (depth>0)&(u>=0)&(u<=1)&(v>=0)&(v<=1)
    opacity = np.zeros(depth.shape, np.float32)
    if np.any(mask):
        x = u[mask]*(image.shape[1]-1); y = v[mask]*(image.shape[0]-1)
        x0, y0 = np.floor(x).astype(int), np.floor(y).astype(int)
        x1, y1 = np.minimum(x0+1, image.shape[1]-1), np.minimum(y0+1, image.shape[0]-1)
        fx, fy = (x-x0)[:, None], (y-y0)[:, None]
        sample = (image[y0,x0]*(1-fx)+image[y0,x1]*fx)*(1-fy)+(image[y1,x0]*(1-fx)+image[y1,x1]*fx)*fy
        # Feather only the glow's outermost canvas edge, so an opaque/cropped
        # source halo can never become a rectangle on the continuous sky sphere.
        fade = smooth(0, .075, u[mask])*smooth(0,.075,1-u[mask])*smooth(0,.075,v[mask])*smooth(0,.075,1-v[mask])
        a = np.clip(sample[:,3]*fade,0,1)
        rgb[mask] = rgb[mask]*(1-a[:,None]) + sample[:,:3]**2.2*a[:,None]*1.35
        opacity[mask] = a
    return rgb, opacity


def sky(d, kind, seed):
    x, y, z = d[..., 0], d[..., 1], d[..., 2]
    positive = np.maximum(z, 0)
    night = kind in ('night', 'aurora', 'space')
    presets = {
        'sunny': ((.075, .30, .66), (.75, .86, .94), (.27, .40, .57)),
        'night': ((.009, .019, .055), (.10, .16, .25), (.018, .030, .060)),
        'sunset': ((.15, .075, .29), (.95, .46, .23), (.11, .065, .17)),
        'overcast': ((.27, .34, .41), (.70, .74, .76), (.24, .30, .34)),
        'aurora': ((.006, .018, .038), (.055, .12, .16), (.008, .022, .032)),
        'space': ((.006, .008, .023), (.012, .014, .035), (.006, .008, .020)),
    }
    zenith, horizon, bottom = (linear(v) for v in presets[kind])
    blend = smooth(0, 1, positive)**.56
    rgb = horizon+(zenith-horizon)*blend[..., None]
    below = smooth(0, .85, np.maximum(-z, 0))
    rgb = rgb*(1-below[..., None])+bottom*below[..., None]
    visibility = np.ones(z.shape, np.float32)
    sun = celestial(.78, .70 if kind == 'sunny' else .048)
    sun_dot = np.clip(np.sum(d*sun, axis=-1), -1, 1)
    sun_angle2 = np.maximum(0, 2*(1-sun_dot))
    if kind in ('sunny', 'sunset'):
        sunset = kind == 'sunset'
        warmth = linear((1.0, .65 if sunset else .87, .30 if sunset else .64))
        halo = np.exp(-sun_angle2/(.28 if sunset else .12)**2)
        rgb += warmth*halo[..., None]*(.80 if sunset else .28)
        disk = np.exp(-(sun_angle2/(.017 if sunset else .026)**2)**3)
        rgb += linear((1, .90, .64))*disk[..., None]*7
        rgb += warmth*np.exp(-sun_angle2/.003)[..., None]*.45
    if kind == 'overcast':
        diffuse = celestial(-.8, .51)
        rgb += linear((.70, .76, .83))*np.exp(-(1-np.sum(d*diffuse, axis=-1))/.24)[..., None]*.12
    if kind == 'space':
        normal = unit((.31, -.47, .83))
        latitude = np.arcsin(np.clip(np.sum(d*normal, axis=-1), -1, 1))
        band = np.exp(-(latitude/.24)**2)
        a = fbm(x*7.0+3, y*7-2, z*7+8, seed, 6)
        b = fbm(x*15-3, y*15+4, z*15+11, seed+419, 4)
        gas = np.maximum(0, a-.31)**1.55*band*5.8
        dust = smooth(.45, .63, b)
        violet, blue = linear((.58, .20, .75)), linear((.13, .49, .76))
        hue = smooth(.35, .65, fbm(x*3+8, y*3+1, z*3+7, seed+129, 3))
        rgb += gas[..., None]*(violet+(blue-violet)*hue[..., None])*(1-.70*dust[..., None])
        rgb += linear((.86, .70, .55))*(band*np.maximum(0, a-.43)*.35)[..., None]
        visibility *= 1-.5*dust*band
        return rgb, visibility
    # Multiscale cloud mass in a direction-continuous, horizontal cloud layer.
    qx, qy = x/(np.abs(z)+.20), y/(np.abs(z)+.20)
    qz = z*.85+4.1
    warp = noise(qx*.7+1, qy*.7-3, qz, seed+913)-.5
    a = fbm(qx*1.45+warp*.75+6, qy*1.45+warp*.65-4, qz, seed, 5)
    detail = fbm(qx*5.3-4, qy*5.3+7, qz*2.1, seed+137, 3)
    mass = .79*a+.21*detail
    threshold = {'sunny': .55, 'night': .64, 'sunset': .54, 'overcast': .39, 'aurora': .69}[kind]
    alpha = smooth(threshold-.11, threshold+.12, mass)
    fade = smooth(-.035, .20, z)
    alpha *= fade
    if kind in ('sunny', 'sunset'):
        clearing = .065 if kind == 'sunset' else .28
        alpha *= 1-.98*np.exp(-sun_angle2/clearing**2)
    shade = np.clip(.55+.85*a+.50*(detail-.5), .48, 1.20)
    if kind == 'sunny':
        cloud = linear((.95, .97, 1.0))*shade[..., None]
        cloud += linear((1.0, .84, .61))*(np.exp(-sun_angle2/.25)*(1-alpha)*.75)[..., None]
    elif kind == 'sunset':
        warm = np.exp(-sun_angle2/.75)
        cloud = linear((.31, .25, .39))*(shade*.70)[..., None]
        cloud += linear((1.0, .53, .21))*(warm*(.35+.65*shade)*(1-alpha*.55))[..., None]
    elif kind == 'overcast':
        cloud = linear((.63, .68, .73))*np.clip(.34+shade*.55+detail*.23, .35, 1)[..., None]
        alpha = np.minimum(.98, .50+.50*alpha)*smooth(-.03, .16, z)
    else:
        cloud = linear((.19, .25, .38))*(shade*.58)[..., None]
        alpha *= .55 if kind == 'night' else .30
    rgb = rgb*(1-alpha[..., None])+cloud*alpha[..., None]
    visibility *= 1-alpha*.96
    if kind in ('sunny', 'sunset', 'night'):
        cirrus = fbm(x*7+4, y*2.3-8, z*15+3, seed+544, 3)
        cirrus = smooth(.49, .73, cirrus)*smooth(.08, .65, z)*(.022 if night else .055)
        rgb += linear((.67, .74, .87))*cirrus[..., None]
    if kind == 'night':
        rgb, emblem_opacity = shoe_in_sky(d, rgb)
        visibility *= 1-emblem_opacity
    if kind == 'aurora':
        phi = np.arctan2(y, x)
        elevation = np.arcsin(np.clip(z, -1, 1))
        curtain = np.zeros(z.shape, np.float32)
        color = np.zeros(d.shape, np.float32)
        for shift, weight in ((0, 1), (.16, .43), (.31, .18)):
            base = .30+.11*np.sin(phi*2+.7)+.055*np.sin(phi*5-1.2)+.024*np.sin(phi*11+1.7)+.010*np.sin(phi*23-.4)+shift*(1+.25*np.sin(phi*3+shift*9))
            height = elevation-base
            width = .018+.020*(.5+.5*np.sin(phi*7+.3*np.sin(phi*13)))
            band = np.exp(-(height/width)**2)
            ray_noise = fbm(np.cos(phi)*55+.65*np.sin(elevation*7), np.sin(phi)*55, elevation*3+5+shift*9, seed+1391, 3)
            rays = .16+.84*smooth(.27, .73, ray_noise)
            top = .11+.22*noise(np.cos(phi)*5+3, np.sin(phi)*5+7, z*0+4, seed+1589)
            vertical = smooth(-.025, .035, height)*np.exp(-(np.maximum(height, 0)/top)**1.5)*rays
            variation = noise(np.cos(phi)*3+5, np.sin(phi)*3+7, z*0+4, seed+731)
            envelope = (.13+.87*(.5+.5*np.cos(phi-.9))**3)*(.35+.85*variation)
            energy = (band*.23+vertical*.90)*envelope*weight
            purple = smooth(.055, .30, height)
            low, high = linear((.06, .95, .48)), linear((.65, .22, .93))
            color += energy[..., None]*(low+(high-low)*purple[..., None])*1.5
            curtain += energy
        rgb += color
        visibility *= 1-np.clip(curtain*.18, 0, .4)
    return np.maximum(rgb, 0), visibility


def star_catalog(kind, seed):
    counts = {'sunny': 0, 'sunset': 0, 'overcast': 0, 'night': 2200, 'aurora': 3200, 'space': 8500}
    count = counts[kind]
    rng = np.random.default_rng(seed+2207)
    z = rng.uniform(-1, 1, count)
    phi = rng.uniform(-math.pi, math.pi, count)
    directions = np.stack((np.sqrt(1-z*z)*np.cos(phi), np.sqrt(1-z*z)*np.sin(phi), z), axis=-1).astype(np.float32)
    brightness = 10**rng.uniform(-1.45, .20, count)
    sigma = rng.uniform(.00040, .00068, count)
    colors = np.empty((count, 3), np.float32)
    t = rng.random(count)
    colors[:] = linear((.88, .93, 1.0))
    warm, blue = t < .22, t > .73
    colors[warm] = linear((1, .76, .48))
    colors[blue] = linear((.58, .77, 1))
    if count:
        brightness[:12] *= 3
        sigma[:12] *= 1.35
    return directions, brightness, sigma, colors


def stars_on_face(hdr, visibility, face, catalog):
    n, right, up = (np.asarray(v, np.float32) for v in FACES[face])
    points, brightness, sigmas, colors = catalog
    size = hdr.shape[0]
    for star, energy, sigma, color in zip(points, brightness, sigmas, colors):
        depth = float(np.dot(star, n))
        if depth <= .45:
            continue
        sx, ty = float(np.dot(star, right))/depth, float(np.dot(star, up))/depth
        if abs(sx) > 1.04 or abs(ty) > 1.04:
            continue
        px, py = (sx+1)*(size-1)/2, (1-ty)*(size-1)/2
        radius = max(3, math.ceil(sigma*(size-1)/2/depth**2*7))
        x0, x1 = max(0, math.floor(px-radius)), min(size, math.ceil(px+radius+1))
        y0, y1 = max(0, math.floor(py-radius)), min(size, math.ceil(py+radius+1))
        if x1 <= x0 or y1 <= y0:
            continue
        xx, yy = np.meshgrid(np.arange(x0, x1, dtype=np.float32), np.arange(y0, y1, dtype=np.float32))
        d = face_directions(face, xx*2/(size-1)-1, 1-yy*2/(size-1))
        distance = np.sum((d-star)**2, axis=-1)
        profile = np.exp(-distance/(sigma*sigma)*.5)
        if energy > 1:
            profile += .035*np.exp(-distance/(sigma*sigma*12)*.5)
        hdr[y0:y1, x0:x1] += profile[..., None]*color*energy*visibility[y0:y1, x0:x1, None]


def tonemap(hdr):
    hdr = np.maximum(hdr, 0)*1.08
    mapped = np.clip((hdr*(2.51*hdr+.03))/(hdr*(2.43*hdr+.59)+.14), 0, 1)
    return np.rint(mapped**(1/2.2)*255).astype(np.uint8)


def render_face(face, size, kind, seed, catalog):
    # Symmetric coordinate endpoints guarantee exact corresponding edge vectors.
    coordinates = np.linspace(-1, 1, size, dtype=np.float32)
    hdr = np.empty((size, size, 3), np.float32)
    visibility = np.empty((size, size), np.float32)
    for begin in range(0, size, 96):
        end = min(begin+96, size)
        s, t = np.meshgrid(coordinates, -coordinates[begin:end])
        hdr[begin:end], visibility[begin:end] = sky(face_directions(face, s, t), kind, seed)
    stars_on_face(hdr, visibility, face, catalog)
    assert np.isfinite(hdr).all()
    return tonemap(hdr)


def edge_groups(size):
    coordinates = np.linspace(-1, 1, size, dtype=np.float32)
    groups = {}
    for face in FACES:
        for edge in ('left', 'right', 'top', 'bottom'):
            if edge in ('left', 'right'):
                s, t = np.full(size, -1 if edge == 'left' else 1, np.float32), -coordinates
            else:
                s, t = coordinates, np.full(size, 1 if edge == 'top' else -1, np.float32)
            d = face_directions(face, s, t)
            endpoints = [tuple(np.round(d[0], 6)), tuple(np.round(d[-1], 6))]
            key = tuple(sorted(endpoints))
            groups.setdefault(key, []).append((face, edge, endpoints[0] == key[0]))
    assert len(groups) == 12 and all(len(v) == 2 for v in groups.values())
    return groups


def edge_pixels(image, edge):
    return {'left': image[:, 0], 'right': image[:, -1], 'top': image[0], 'bottom': image[-1]}[edge]


def stitch_and_check(images):
    groups = edge_groups(next(iter(images.values())).shape[0])
    maximum = 0
    for pair in groups.values():
        strips = [edge_pixels(images[f], e)[::1 if forward else -1] for f, e, forward in pair]
        difference = np.abs(strips[0].astype(np.int16)-strips[1].astype(np.int16))
        maximum = max(maximum, int(difference.max()))
        # Native texels share one quantized boundary; interiors are untouched.
        shared = np.rint((strips[0].astype(np.float32)+strips[1])/2).astype(np.uint8)
        strips[0][1:-1] = shared[1:-1]
        strips[1][1:-1] = shared[1:-1]
    # Three incident faces must share each of the eight corner texels too.
    corners = {}
    for face in images:
        for x in (0, -1):
            for y in (0, -1):
                d = face_directions(face, np.float32(-1 if x == 0 else 1), np.float32(1 if y == 0 else -1))
                corners.setdefault(tuple(np.sign(d).astype(int)), []).append((face, y, x))
    assert len(corners) == 8 and all(len(v) == 3 for v in corners.values())
    for group in corners.values():
        color = np.rint(np.mean([images[f][y, x] for f, y, x in group], axis=0)).astype(np.uint8)
        for face, y, x in group:
            images[face][y, x] = color
    post = 0
    for pair in groups.values():
        a, b = [edge_pixels(images[f], e)[::1 if forward else -1] for f, e, forward in pair]
        post = max(post, int(np.abs(a.astype(np.int16)-b.astype(np.int16)).max()))
    assert post == 0
    if maximum > 5:
        raise RuntimeError(f'Unexpected discontinuity before quantization stitching: {maximum}')
    return {'edge_pairs': 12, 'corners': 8, 'pre_quantization_edge_max_rgb8': maximum, 'final_edge_max_rgb8': post}


def sample_cube(images, d):
    output = np.zeros(d.shape, np.uint8)
    dominant = np.argmax(np.abs(d), axis=-1)
    size = next(iter(images.values())).shape[0]
    for face, (normal, right, up) in FACES.items():
        n = np.asarray(normal, np.float32)
        component = np.argmax(np.abs(n))
        mask = (dominant == component) & ((d[..., component] > 0) == (n[component] > 0))
        denominator = np.sum(d[mask]*n, axis=-1)
        s = np.sum(d[mask]*np.asarray(right), axis=-1)/denominator
        t = np.sum(d[mask]*np.asarray(up), axis=-1)/denominator
        x = np.clip((s+1)*.5*(size-1), 0, size-1)
        y = np.clip((1-t)*.5*(size-1), 0, size-1)
        x0, y0 = np.floor(x).astype(int), np.floor(y).astype(int)
        x1, y1 = np.minimum(x0+1, size-1), np.minimum(y0+1, size-1)
        u, v = x-x0, y-y0
        im = images[face]
        output[mask] = np.rint((im[y0, x0]*(1-u[:, None])+im[y0, x1]*u[:, None])*(1-v[:, None])+(im[y1, x0]*(1-u[:, None])+im[y1, x1]*u[:, None])*v[:, None]).astype(np.uint8)
    return output



def panorama(images, width=1536, height=768):
    phi, lat = np.meshgrid(np.linspace(-math.pi, math.pi, width), np.linspace(math.pi/2, -math.pi/2, height))
    d = np.stack((np.cos(lat)*np.cos(phi), np.cos(lat)*np.sin(phi), np.sin(lat)), axis=-1).astype(np.float32)
    return sample_cube(images, d)


def night_preview(images):
    forward = celestial(1.27,.50)
    right = unit(np.cross(forward,np.array((0,0,1),np.float32)))
    up = np.cross(right,forward)
    x,y = np.meshgrid(np.linspace(-1.0,1.0,512),np.linspace(.5,-.5,256))
    d = unit(forward+x[...,None]*right+y[...,None]*up).astype(np.float32)
    return sample_cube(images,d)


def tga(path, pixels):
    height, width = pixels.shape[:2]
    header = struct.pack('<BBBHHBHHHHBB', 0, 0, 2, 0, 0, 0, 0, 0, width, height, 24, 0)
    path.write_bytes(header+pixels[::-1, :, ::-1].tobytes())
    decoded = np.asarray(Image.open(path).convert('RGB'))
    assert np.array_equal(decoded, pixels)


def generate(output, art, size, selected):
    if size < 128 or size > 2048 or size & (size-1):
        raise ValueError('Face size must be a power of two from 128 to 2048.')
    unknown = selected-{p[0] for p in CATALOG}
    if unknown:
        raise ValueError(f'Unknown preset IDs: {sorted(unknown)}')
    output.mkdir(parents=True, exist_ok=True)
    art.mkdir(parents=True, exist_ok=True)
    previous = {}
    if selected and (output/'manifest.json').exists():
        previous_manifest = json.loads((output/'manifest.json').read_text(encoding='utf-8'))
        if previous_manifest['format']['width'] != size:
            raise ValueError('Partial generation must use the existing face size; regenerate all presets to change it.')
        previous = {p['id']: p for p in previous_manifest['presets']}
    manifest = {'schema': 1, 'license': 'CC0-1.0', 'generation': 'Original deterministic procedural mathematics; no external images.',
                'format': {'width': size, 'height': size, 'bits': 24, 'compression': 'none', 'tga_origin': 'bottom-left', 'decoded_origin': 'top-left'},
                'face_order': list(FACES), 'directions': {'rt': '(1,-s,t)', 'bk': '(s,1,t)', 'lf': '(-1,s,t)', 'ft': '(-s,-1,t)', 'up': '(-t,-s,1)', 'dn': '(t,-s,-1)'},
                'coordinates': 's=2u-1; t=1-2v; row0 is visual top; normalize direction', 'presets': []}
    for ident, name, description, seed in CATALOG:
        if selected and ident not in selected:
            continue
        start = time.monotonic()
        catalog = star_catalog(ident, seed)
        images = {}
        for face in FACES:
            images[face] = render_face(face, size, ident, seed, catalog)
            print(f'{ident}/{face}: rendered', flush=True)
        seams = stitch_and_check(images)
        folder = output/ident
        folder.mkdir(parents=True, exist_ok=True)
        files = {}
        for face, pixels in images.items():
            path = folder/f'{face}.tga'
            tga(path, pixels)
            files[face] = {'path': f'{ident}/{face}.tga', 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'bytes': path.stat().st_size}
        pano = panorama(images)
        Image.fromarray(pano).save(art/f'{ident}_panorama.png')
        preview = np.asarray(Image.fromarray(pano).resize((512, 256), Image.Resampling.LANCZOS))
        if ident == 'night':
            preview = night_preview(images)
        tga(folder/'preview.tga', preview)
        Image.fromarray(preview).save(art/f'{ident}_preview.png')
        cube = Image.new('RGB', (size*4, size*3), (16, 18, 24))
        positions = {'up': (1, 0), 'rt': (0, 1), 'bk': (1, 1), 'lf': (2, 1), 'ft': (3, 1), 'dn': (1, 2)}
        for face, (x, y) in positions.items():
            cube.paste(Image.fromarray(images[face]), (x*size, y*size))
        cube.thumbnail((1600, 1200), Image.Resampling.LANCZOS)
        cube.save(art/f'{ident}_cube.png')
        all_pixels = np.concatenate([im.reshape(-1, 3)[::97] for im in images.values()])
        entry = {'id': ident, 'name_ru': name, 'description_ru': description, 'seed': seed, 'star_count': len(catalog[0]), 'faces': files,
                 'preview': {'path': f'{ident}/preview.tga', 'width': 512, 'height': 256}, 'seams': seams,
                 'mean_rgb8': np.mean(all_pixels, axis=0).round(2).tolist(), 'std_rgb8': np.std(all_pixels, axis=0).round(2).tolist()}
        manifest['presets'].append(entry)
        print(f'{ident}: complete in{time.monotonic()-start:.1f}s; seams{seams}', flush=True)
    if previous:
        previous.update({p['id']: p for p in manifest['presets']})
        manifest['presets'] = [previous[ident] for ident, *_ in CATALOG if ident in previous]
    (output/'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    return manifest


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[1]/'baloncuaga_skyboxes')
    parser.add_argument('--art', type=Path, default=Path(__file__).resolve().parent/'skybox_previews')
    parser.add_argument('--size', type=int, default=1024)
    parser.add_argument('--only', nargs='*')
    args = parser.parse_args()
    generate(args.output, args.art, args.size, set(args.only or []))
