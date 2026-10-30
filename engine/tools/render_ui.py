#!/usr/bin/env python3
"""Draws the PhoneUI ops (tools/phone_render) like android DawView does. usage: render_ui.py ui_ops.json outdir [scale]"""
import json, sys, os
from PIL import Image, ImageDraw, ImageFont
d = json.load(open(sys.argv[1])); out = sys.argv[2]; Z = float(sys.argv[3]) if len(sys.argv) > 3 else 2.0
os.makedirs(out, exist_ok=True)
SS = 2                                            # supersampling for anti-aliasing
K = Z * SS
reg = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'; bold = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
_fc = {}
def font(sz, b):
    k = (round(sz * K), b)
    if k not in _fc: _fc[k] = ImageFont.truetype(bold if b else reg, max(6, round(sz * K * 0.92)))
    return _fc[k]
def rgba(c): return ((c >> 16) & 255, (c >> 8) & 255, c & 255, (c >> 24) & 255)
def render(screen):
    W, H = d['W'], d['H']
    img = Image.new('RGB', (int(W * K), int(H * K)), (0, 0, 0))
    layers = [(img, ImageDraw.Draw(img, 'RGBA'), None)]
    for t, a, b, c, e, f, g, col, s in screen['ops']:
        img_, dr, clipr = layers[-1]
        if t in (0, 1) and (c * K < 2 or e * K < 2): continue
        if t == 0: dr.rounded_rectangle((a * K, b * K, (a + c) * K - 1, (b + e) * K - 1), radius=f * K, fill=rgba(col))
        elif t == 1: dr.rounded_rectangle((a * K, b * K, (a + c) * K - 1, (b + e) * K - 1), radius=f * K, outline=rgba(col), width=max(1, round(g * K)))
        elif t == 2:
            fn = font(c, bool(f)); anchor = {0: 'ls', 1: 'ms', 2: 'rs'}[int(e)]
            dr.text((a * K, b * K), s, font=fn, fill=rgba(col), anchor=anchor)
        elif t == 3: dr.line((a * K, b * K, c * K, e * K), fill=rgba(col), width=max(1, round(f * K)))
        elif t == 4: dr.ellipse(((a - c) * K, (b - c) * K, (a + c) * K, (b + c) * K), fill=rgba(col))
        elif t == 5: dr.polygon([(a * K, b * K), (c * K, e * K), (f * K, g * K)], fill=rgba(col))
        elif t == 6:   # clip: draw on a fresh transparent layer, composite cropped
            L = Image.new('RGBA', img.size, (0, 0, 0, 0)); layers.append((L, ImageDraw.Draw(L, 'RGBA'), (a, b, c, e)))
        elif t == 7:
            L, _, r = layers.pop(); x, y, w, h = r
            box = (int(x * K), int(y * K), int((x + w) * K), int((y + h) * K))
            base = layers[-1][0]
            if base.mode == 'RGB': base.paste(L.crop(box), (box[0], box[1]), L.crop(box))
            else: base.alpha_composite(L.crop(box), (box[0], box[1]))
    return layers[0][0].convert('RGB').resize((int(W * Z), int(H * Z)), Image.LANCZOS)
ims = []
for sc in d['screens']:
    im = render(sc); im.save(os.path.join(out, sc['name'] + '.png')); ims.append((sc['name'], im))
print([n for n, _ in ims])
