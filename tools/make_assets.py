# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 GuitarFros - Doctor Migrana - RockON 3-in-1
"""Regenerates the GUI background and the small sprites from the original Meta AI image.

Usage (from the repository root):   python3 tools/make_assets.py [--out assets] [--docs docs]

Needs: python3, numpy, opencv-python, pillow and the DejaVu Sans Condensed Bold font.
The result is only used at build time through assets/ (they are already committed), so you only need this
script if you want to change the artwork.
"""
import argparse, os, sys
import cv2, numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
ap = argparse.ArgumentParser()
ap.add_argument('--out', default=os.path.join(ROOT, 'assets'))
ap.add_argument('--docs', default=os.path.join(ROOT, 'docs'))
ARGS = ap.parse_args()
SRC_PATH = os.path.join(ROOT, 'art_source', 'doctor_migrana_source.webp')
OUT = ARGS.out
os.makedirs(OUT, exist_ok=True); os.makedirs(ARGS.docs, exist_ok=True)
src = cv2.imread(SRC_PATH)
if src is None:
    src = cv2.cvtColor(np.array(Image.open(SRC_PATH).convert('RGB')), cv2.COLOR_RGB2BGR)
H, W = src.shape[:2]
bg = src.copy()
gray = cv2.cvtColor(src, cv2.COLOR_BGR2GRAY)

# ------------------------------------------------------------------ 1) text removal (inpaint) ------------
mask = np.zeros((H, W), np.uint8)
def bright_box(x0, y0, x1, y1, thr, grow=5):
    m = np.zeros((H, W), np.uint8)
    m[y0:y1, x0:x1] = (gray[y0:y1, x0:x1] > thr).astype(np.uint8) * 255
    return cv2.dilate(m, np.ones((grow, grow), np.uint8))
for box, thr, grow in [((488, 522, 635, 582), 110, 5),   # BYPASS BYPASS
                       ((528, 398, 552, 424), 110, 5),   # ON
                       ((1404, 184, 1612, 212), 100, 5), # preset name
                       ((1528, 126, 1572, 153), 100, 5), # A/B
                       ((308, 1078, 460, 1108), 100, 5), # INPUT
                       ((1460, 1078, 1605, 1108), 100, 5),# OUTPUT
                       ((1018, 824, 1110, 868), 100, 7), # old DRIVE label
                       ((1236, 824, 1324, 868), 100, 7), # old TONE label
                       ((1436, 824, 1582, 864), 100, 7), # old MASTER label
                       ((1468, 862, 1548, 892), 100, 7), # old VOLUME label
                       ((255, 140, 815, 192), 62, 7),    # title
                       ((672, 1160, 1244, 1184), 105, 5)]:   # footer line
    mask |= bright_box(*box, thr, grow)
bg = cv2.inpaint(bg, mask, 6, cv2.INPAINT_TELEA)

# ------------------------------------------------------------------ 2) LED clean-up (as before) ----------
def clone_patch(img, led, srcpt, ax, ay):
    m = np.zeros(img.shape[:2], np.uint8)
    cv2.ellipse(m, srcpt, (ax, ay), 0, 0, 360, 255, -1)
    return cv2.seamlessClone(img.copy(), img, m, led, cv2.NORMAL_CLONE)
bg = clone_patch(bg, (640, 903), (800, 903), 52, 33)
bg = clone_patch(bg, (958, 903), (800, 903), 52, 33)
bg = clone_patch(bg, (1274, 903), (1130, 903), 52, 33)
bg = clone_patch(bg, (538, 377), (420, 377), 48, 48)

def neutralise(img, cx, cy, rx, ry):
    m = np.zeros(img.shape[:2], np.float32)
    cv2.ellipse(m, (cx, cy), (rx, ry), 0, 0, 360, 1.0, -1)
    m = cv2.GaussianBlur(m, (0, 0), 8)[..., None]
    f = img.astype(np.float32)
    B, G, R = f[..., 0], f[..., 1], f[..., 2]
    n = np.stack([B, np.minimum(G, B + 3), np.minimum(R, B + 3)], -1)
    return (f * (1 - m) + n * m).astype(np.uint8)
for (x, y) in [(640, 905), (958, 905), (1274, 905)]:
    bg = neutralise(bg, x, y, 85, 45)
bg = neutralise(bg, 538, 377, 60, 60)

def slot_copy(img, dx0, dx1, sx0):
    y0, y1 = 1106, 1142
    w = dx1 - dx0
    seg = img[y0:y1, sx0:sx0 + w].astype(np.float32)
    a = np.ones((y1 - y0, w), np.float32); f = 10
    a[:, :f] = np.linspace(0, 1, f); a[:, -f:] = np.linspace(1, 0, f)
    a = a[..., None]
    out = img.astype(np.float32)
    out[y0:y1, dx0:dx1] = seg * a + out[y0:y1, dx0:dx1] * (1 - a)
    return out.astype(np.uint8)
bg = slot_copy(bg, 316, 540, 720)
bg = slot_copy(bg, 1380, 1604, 800)

rng = np.random.default_rng(1)
noise = rng.normal(0, 3.0, bg.shape).astype(np.float32)
m3 = (cv2.GaussianBlur(mask, (7, 7), 0) / 255.0)[..., None]
bg = np.clip(bg.astype(np.float32) + noise * m3, 0, 255).astype(np.uint8)

# ------------------------------------------------------------------ 3) layout helpers -------------------
def feather_rect_mask(w, h, edge):
    m = np.zeros((h, w), np.float32)
    cv2.rectangle(m, (edge, edge), (w - edge - 1, h - edge - 1), 1.0, -1)
    return cv2.GaussianBlur(m, (0, 0), edge / 2.0)[..., None]

def blend_rect(dst, crop, dx0, dy0, edge):
    h, w = crop.shape[:2]
    m = feather_rect_mask(w, h, edge)
    roi = dst[dy0:dy0 + h, dx0:dx0 + w].astype(np.float32)
    dst[dy0:dy0 + h, dx0:dx0 + w] = (crop.astype(np.float32) * m + roi * (1 - m)).astype(np.uint8)
    return dst

def poisson_patch(dst, patch, dx0, dy0, inset=5):
    h, w = patch.shape[:2]
    m = np.zeros((h, w), np.uint8)
    m[inset:h - inset, inset:w - inset] = 255
    return cv2.seamlessClone(np.ascontiguousarray(patch), dst, m, (dx0 + w // 2, dy0 + h // 2), cv2.NORMAL_CLONE)

def restore_region(dst, dx0, dy0, w, h, sx0, sy0, flip=None, edge=5):
    """cover a region with clean panel texture taken from the original image (Poisson-blended)"""
    patch = src[sy0:sy0 + h, sx0:sx0 + w]
    if flip == 'h': patch = patch[:, ::-1]
    if flip == 'v': patch = patch[::-1, :]
    if flip == 'hv': patch = patch[::-1, ::-1]
    return poisson_patch(dst, patch, dx0, dy0, edge)

def paste_disc(dst, cx, cy, R, dx, dy, scale):
    crop = src[cy - R:cy + R, cx - R:cx + R].astype(np.float32)
    m = np.zeros((2 * R, 2 * R), np.float32)
    cv2.circle(m, (R, R), R - 10, 1.0, -1)
    m = cv2.GaussianBlur(m, (0, 0), 5)
    ns = int(round(2 * R * scale))
    if ns != 2 * R:
        crop = cv2.resize(crop, (ns, ns), interpolation=cv2.INTER_AREA)
        m = cv2.resize(m, (ns, ns), interpolation=cv2.INTER_AREA)
    m = m[..., None]
    x0, y0 = dx - ns // 2, dy - ns // 2
    roi = dst[y0:y0 + ns, x0:x0 + ns].astype(np.float32)
    dst[y0:y0 + ns, x0:x0 + ns] = (crop * m + roi * (1 - m)).astype(np.uint8)
    return dst

# ------------------------------------------------------------------ 4) restore areas that change --------
bg = restore_region(bg, 283, 618, 170, 250, 1282, 338)                    # old MODE / SERIES blocks
bg = restore_region(bg, 470, 445, 150, 100, 1300, 440, flip='hv', edge=6) # old ON/BYPASS lever
bg = restore_region(bg, 958, 612, 216, 216, 1296, 350, flip='h')          # old DRIVE plate (-> smaller COMP LEVEL)
bg = restore_region(bg, 1398, 606, 242, 236, 1296, 345, flip='v')         # old MASTER plate
bg = restore_region(bg, 514, 604, 232, 224, 1296, 350, flip='hv')         # old BOOST plate
bg = restore_region(bg, 730, 604, 232, 224, 1290, 345)                    # old COMP plate
bg = restore_region(bg, 1165, 604, 232, 224, 1300, 345, flip='h')         # old TONE plate (-> DRIVE)

# ------------------------------------------------------------------ 5) link switch blocks ----------------
# source blocks (LED + switch base + labels) with their old labels erased
src_work = src.copy()
lab_mask = np.zeros((H, W), np.uint8)
for (x0, y0, x1, y1) in [(295, 698, 430, 724), (295, 836, 430, 858)]:
    lab_mask[y0:y1, x0:x1] = (gray[y0:y1, x0:x1] > 110).astype(np.uint8) * 255
lab_mask = cv2.dilate(lab_mask, np.ones((5, 5), np.uint8))
src_work = cv2.inpaint(src_work, lab_mask, 4, cv2.INPAINT_TELEA)
DX = 174
bg = poisson_patch(bg, src_work[630:730, 292:440], 292 + DX, 360, 6)    # block 1  (dy -270)
bg = poisson_patch(bg, src_work[764:864, 292:440], 292 + DX, 474, 6)    # block 2  (dy -290)

# ON/OFF LED housing: same bezel as the other LEDs, at the bypass LED position
def paste_led_housing(dst, sx, sy, dx, dy, r=22):
    crop = src[sy - r:sy + r, sx - r:sx + r].astype(np.float32)
    m = np.zeros((2 * r, 2 * r), np.float32)
    cv2.circle(m, (r, r), r - 3, 1.0, -1)
    m = cv2.GaussianBlur(m, (0, 0), 1.6)[..., None]
    x0, y0 = dx - r, dy - r
    roi = dst[y0:y0 + 2 * r, x0:x0 + 2 * r].astype(np.float32)
    dst[y0:y0 + 2 * r, x0:x0 + 2 * r] = (crop * m + roi * (1 - m)).astype(np.uint8)
    return dst
bg = paste_led_housing(bg, 322, 662, 364, 377)

# ------------------------------------------------------------------ 6) realistic knobs ------------
sys.path.insert(0, HERE)
from knob_render import render_cap
def find_font():
    for p in ('/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf',
              '/usr/share/fonts/dejavu/DejaVuSansCondensed-Bold.ttf',
              '/Library/Fonts/DejaVuSansCondensed-Bold.ttf',
              os.path.expanduser('~/Library/Fonts/DejaVuSansCondensed-Bold.ttf'),
              'C:/Windows/Fonts/DejaVuSansCondensed-Bold.ttf'):
        if os.path.exists(p):
            return p
    sys.exit('DejaVuSansCondensed-Bold.ttf not found: install the DejaVu fonts (package fonts-dejavu-core).')
FONT_N = find_font()
KNOBS = [  # id           cx    cy   scale
         ('range',      407, 720, 0.62), ('boost',     627, 720, 1.00), ('comp',      846, 720, 1.00),
         ('compLevel', 1066, 720, 0.62), ('drive',    1288, 720, 1.00), ('driveTone', 1452, 665, 0.62),
         ('driveLevel',1568, 785, 0.62), ('tone',     1390, 425, 0.70), ('master',    1550, 425, 0.70)]
RK_FULL = 58.0
TICK_RGB = (206, 197, 170)

def draw_knob(img, cx, cy, Rk):
    W_, H_ = img.size
    # soft shadows on the panel (contact + cast, light from the top-left)
    sh = Image.new('RGBA', img.size, (0, 0, 0, 0)); d = ImageDraw.Draw(sh)
    ox, oy = Rk * 0.10, Rk * 0.16
    d.ellipse((cx - Rk + ox, cy - Rk + oy, cx + Rk + ox, cy + Rk + oy), fill=(0, 0, 0, 200))
    sh1 = sh.filter(ImageFilter.GaussianBlur(Rk * 0.20))
    sh2 = Image.new('RGBA', img.size, (0, 0, 0, 0)); d2 = ImageDraw.Draw(sh2)
    d2.ellipse((cx - Rk * 1.04 + Rk * 0.03, cy - Rk * 1.04 + Rk * 0.05, cx + Rk * 1.04 + Rk * 0.03, cy + Rk * 1.04 + Rk * 0.05), fill=(0, 0, 0, 170))
    sh2 = sh2.filter(ImageFilter.GaussianBlur(Rk * 0.05))
    img.alpha_composite(sh1); img.alpha_composite(sh2)

    # printed scale: 11 numbered major ticks (0..10) over 270 degrees + minor ticks, starting at 7:30
    layer = Image.new('RGBA', img.size, (0, 0, 0, 0)); dl = ImageDraw.Draw(layer)
    fnt = ImageFont.truetype(FONT_N, max(9, int(round(Rk * 0.28))))
    for i in range(0, 21):                      # 0, 0.5, 1 ... 10
        val = i / 2.0
        ang = np.radians(-135.0 + val * 27.0)
        major = (i % 2 == 0)
        r1 = Rk * 1.28; r2 = Rk * (1.46 if major else 1.38)
        w = max(1, int(round(Rk * (0.05 if major else 0.028))))
        x1, y1 = cx + r1 * np.sin(ang), cy - r1 * np.cos(ang)
        x2, y2 = cx + r2 * np.sin(ang), cy - r2 * np.cos(ang)
        dl.line((x1, y1, x2, y2), fill=TICK_RGB + (230 if major else 170,), width=w)
        if major:
            rn = Rk * 1.72
            dl.text((cx + rn * np.sin(ang), cy - rn * np.cos(ang)), str(int(val)), font=fnt, fill=TICK_RGB + (235,), anchor='mm')
    img.alpha_composite(layer)
    cap = render_cap(Rk)
    img.alpha_composite(cap, (int(round(cx - cap.width / 2)), int(round(cy - cap.height / 2))))

bgpil_k = Image.fromarray(cv2.cvtColor(bg, cv2.COLOR_BGR2RGB)).convert('RGBA')
for _id, cx, cy, sc in KNOBS:
    draw_knob(bgpil_k, cx, cy, RK_FULL * sc)
bg = cv2.cvtColor(np.array(bgpil_k.convert('RGB')), cv2.COLOR_RGB2BGR)

# ------------------------------------------------------------------ 7) baked text -----------------------
FONT = FONT_N
def text_mask(text, cap_h, kx=1.0, big=240):
    f = ImageFont.truetype(FONT, big)
    cap_big = -f.getbbox("H", anchor='ls')[1]
    l, t, r, b = f.getbbox(text, anchor='ls')
    pad = 12
    base = -t + pad
    im = Image.new('L', (r - l + 2 * pad, b - t + 2 * pad), 0)
    ImageDraw.Draw(im).text((pad - l, base), text, font=f, fill=255, anchor='ls')
    sc = cap_h / cap_big
    im = im.resize((max(1, int(round(im.width * sc * kx))), max(1, int(round(im.height * sc)))), Image.LANCZOS)
    bb = im.getbbox()
    im = im.crop((bb[0], 0, bb[2], im.height))
    return im, int(round(base * sc))          # mask (full height), baseline row inside the mask

def calibrate(pairs, cap_h):
    return float(np.mean([w_ / text_mask(t_, cap_h)[0].width for t_, w_ in pairs]))
KX_MAIN  = calibrate((("BOOST", 78), ("COMP", 67), ("DRIVE", 70), ("TONE", 64)), 20)
KX_SMALL = calibrate((("MODE", 45), ("SERIES", 54)), 12)
KX_TITLE = calibrate((("OVERDRIVE 3-in-1 \u2022 BOUTIQUE", 535),), 28)
KX_FOOT  = calibrate((("SERIAL NO. OV3-001 \u2022 9-18V DC \u2022 VST3 / AU / AAX \u2022 \u00a9 2024 BOUTIQUE AUDIO", 556),), 11)
print("stretch main/small/title:", round(KX_MAIN, 3), round(KX_SMALL, 3), round(KX_TITLE, 3))

bgpil = Image.fromarray(cv2.cvtColor(bg, cv2.COLOR_BGR2RGB)).convert('RGB')
def put(text, x, baseline, cap_h, kx, rgb, align='center', blur=0.6, alpha=0.92):
    mk, base = text_mask(text, cap_h, kx)
    mk = mk.filter(ImageFilter.GaussianBlur(blur))
    left = x - mk.width // 2 if align == 'center' else x
    top = baseline - base
    layer = Image.new('RGB', mk.size, rgb)
    bgpil.paste(layer, (left, top), mk.point(lambda v: int(v * alpha)))

LABEL = (200, 182, 148); SMALL = (188, 187, 182)
# knob labels (baseline = centre + cap/2)
put("RANGE",      407, 798, 17, KX_MAIN, LABEL)
put("COMP LEVEL", 1066, 798, 17, KX_MAIN, LABEL)
put("DRIVE",      1288, 855, 20, KX_MAIN, LABEL)
put("TONE",       1390, 513, 17, KX_MAIN, LABEL)
put("MASTER",     1550, 513, 17, KX_MAIN, LABEL)
put("DRIVE TONE", 1452, 743, 17, KX_MAIN, LABEL)
put("DRIVE LEVEL",1568, 861, 17, KX_MAIN, LABEL)
# link switch labels (old MODE text rows: y 703..715 and 838..850 shifted by -270 / -290, x shifted by +174)
put("COMP",  496, 445, 12, KX_SMALL, SMALL)
put("LINK",  565, 445, 12, KX_SMALL, SMALL)
put("DRIVE", 495, 560, 12, KX_SMALL, SMALL)
put("LINK",  566, 560, 12, KX_SMALL, SMALL)

# footer: brand and year of this project
put("SERIAL NO. OV3-001 \u2022 9-18V DC \u2022 VST3 / STANDALONE \u2022 \u00a9 2026 GUITARFROS", 958, 1176, 11, KX_FOOT, (173, 176, 177), blur=0.5, alpha=0.95)

# ON/OFF block labels (old ON text row y 405..417 and BYPASS row y 534..547, moved to the left column)
put("ON",     365, 417, 12, KX_SMALL, SMALL)
put("BYPASS", 366, 547, 12, KX_SMALL, SMALL)

# title: embossed grey letters
title = "RockON 3-in-1 \u2022 BOUTIQUE"
mk, base = text_mask(title, 28, KX_TITLE)
mk_s = mk.filter(ImageFilter.GaussianBlur(0.7))
grad = Image.new('RGB', mk.size)
gp = grad.load()
for yy in range(mk.height):
    t_ = min(1.0, max(0.0, (yy - (base - 28)) / 28.0))
    v = int(172 - 66 * t_)
    for xx in range(mk.width):
        gp[xx, yy] = (v, v, v - 2)
top = 179 - base
bgpil.paste(Image.new('RGB', mk.size, (14, 14, 16)), (267 + 1, top + 2), mk_s.point(lambda v: int(v * 0.55)))   # shadow
bgpil.paste(grad, (267, top), mk_s.point(lambda v: int(v * 0.95)))
bg = cv2.cvtColor(np.array(bgpil), cv2.COLOR_RGB2BGR)
ok_ = cv2.imwrite(f'{OUT}/bg.jpg', bg, [cv2.IMWRITE_JPEG_QUALITY, 92]); print('bg.jpg written:', ok_, bg.shape, bg.dtype)

# ------------------------------------------------------------------ 8) sprites -------------------------
pil = Image.fromarray(cv2.cvtColor(src, cv2.COLOR_BGR2RGB)).convert('RGBA')
box = (480, 452, 604, 535)
lv = pil.crop(box)
feather = Image.new('L', lv.size, 0)
ImageDraw.Draw(feather).rounded_rectangle((4, 4, lv.width - 4, lv.height - 4), 14, fill=255)
feather = feather.filter(ImageFilter.GaussianBlur(4))
on = lv.copy(); on.putalpha(feather); on.save(f'{OUT}/lever_on.png')
off = lv.transpose(Image.FLIP_LEFT_RIGHT); off.putalpha(feather); off.save(f'{OUT}/lever_off.png')
# COMP LINK / DRIVE LINK switches (lever up / down)
for name, sbox in {'mode': (355, 630, 431, 702), 'series': (355, 764, 431, 836)}.items():
    lv2 = pil.crop(sbox)
    f2 = Image.new('L', lv2.size, 0)
    ImageDraw.Draw(f2).rounded_rectangle((3, 3, lv2.width - 3, lv2.height - 3), 12, fill=255)
    f2 = f2.filter(ImageFilter.GaussianBlur(3))
    up = lv2.copy(); up.putalpha(f2); up.save(f'{OUT}/{name}_up.png')
    dn = lv2.transpose(Image.FLIP_TOP_BOTTOM); dn.putalpha(f2); dn.save(f'{OUT}/{name}_down.png')

# ------------------------------------------------------------------ 9) preview ---------------------------
prev = Image.fromarray(cv2.cvtColor(bg, cv2.COLOR_BGR2RGB)).convert('RGBA')
PREV_VALUES = {'range': 1.0, 'boost': .12, 'comp': .2, 'compLevel': .5, 'drive': .45, 'driveTone': .55,
               'driveLevel': .5, 'tone': .7, 'master': .5}
pd = ImageDraw.Draw(prev)
for _id, cx, cy, sc in KNOBS:                     # same geometry as PointerKnob in the plugin
    Rk = RK_FULL * sc; ang = np.radians(-135 + PREV_VALUES[_id] * 270)
    w = Rk * 0.085
    def pt(r, off=0.0, dx=0.0, dy=0.0):
        return (cx + dx + r * np.sin(ang) + off * np.cos(ang), cy + dy - r * np.cos(ang) + off * np.sin(ang))
    for (dx, dy, col) in ((w * 0.25, w * 0.35, (255, 255, 255, 90)), (0, 0, (30, 24, 18, 255))):
        poly = [pt(Rk * 0.18, -w / 2, dx, dy), pt(Rk * 0.18, w / 2, dx, dy), pt(Rk * 0.66, w / 2, dx, dy), pt(Rk * 0.66, -w / 2, dx, dy)]
        pd.polygon(poly, fill=col)
prev.alpha_composite(Image.open(f'{OUT}/lever_on.png'), (306, 452))                # bypass lever (left column)
prev.alpha_composite(Image.open(f'{OUT}/mode_up.png'), (355 + DX, 630 - 270))     # link switch 1
prev.alpha_composite(Image.open(f'{OUT}/series_up.png'), (355 + DX, 764 - 290))   # link switch 2
# simulate the lit ON LED that the plugin draws live (glow + core)
glow = Image.new('RGBA', prev.size, (0, 0, 0, 0)); gd = ImageDraw.Draw(glow)
for rr in range(44, 0, -2):
    a_ = int(150 * (1 - rr / 44.0) ** 1.6)
    gd.ellipse((364 - rr, 377 - rr, 364 + rr, 377 + rr), fill=(255, 40, 30, a_))
gd.ellipse((364 - 11, 377 - 11, 364 + 11, 377 + 11), fill=(255, 170, 150, 255))
gd.ellipse((364 - 7, 377 - 7, 364 + 7, 377 + 7), fill=(255, 215, 200, 255))
prev = Image.alpha_composite(prev, glow.filter(ImageFilter.GaussianBlur(1.5)))
prev = prev.convert('RGB').resize((1440, 960), Image.LANCZOS)
prev.save(os.path.join(ARGS.docs, 'preview.jpg'), quality=88)
print("done")
