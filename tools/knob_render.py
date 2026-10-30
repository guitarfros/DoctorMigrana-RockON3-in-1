# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 GuitarFros - Doctor Migrana - RockON 3-in-1
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

def sstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)

def render_cap(Rk, SS=4, albedo=(240, 226, 192), flutes=72, seed=3):
    """top-down realistic knob: ivory cap with a fluted skirt, domed top and a recessed groove.
    Lighting is fixed (top-left); returns RGBA at Rk*2+margin pixels (already downsampled)."""
    R = Rk * SS
    N = int(2 * R + 8 * SS)
    yy, xx = np.mgrid[0:N, 0:N].astype(np.float64)
    c = (N - 1) / 2.0
    dx, dy = (xx - c) / R, (yy - c) / R
    r = np.hypot(dx, dy); th = np.arctan2(dy, dx)

    # ---- height field (units of R) ----
    rim = np.sqrt(np.clip(1 - ((r - 0.92) / 0.08) ** 2, 0, 1)) * 0.07           # rounded outer rim of the skirt
    h = np.where(r > 0.92, rim, 0.07 + 0.02 * sstep(0.92, 0.74, r))             # skirt rising slightly towards the body
    h += 0.20 * sstep(0.80, 0.66, r)                                            # body rises from the skirt
    h += 0.075 * np.clip(1 - (r / 0.66) ** 2, 0, 1) ** 1.0                      # gently domed top
    h -= 0.030 * np.exp(-((r - 0.77) / 0.016) ** 2)                             # thin groove
    flute = np.cos(flutes * th) * sstep(0.84, 0.90, r) * sstep(1.0, 0.96, r)
    h += 0.006 * flute                                                          # fine fluting on the skirt
    # ---- normals ----
    gy, gx = np.gradient(h * R)
    k = 1.6
    nx, ny, nz = -gx * k, -gy * k, np.ones_like(h)
    nl = np.sqrt(nx ** 2 + ny ** 2 + nz ** 2); nx, ny, nz = nx / nl, ny / nl, nz / nl
    L = np.array([-0.55, -0.62, 0.56]); L /= np.linalg.norm(L)
    Hh = L + np.array([0, 0, 1.0]); Hh /= np.linalg.norm(Hh)
    ndl = nx * L[0] + ny * L[1] + nz * L[2]
    ndh = nx * Hh[0] + ny * Hh[1] + nz * Hh[2]
    diff = np.clip(ndl, 0, 1)
    spec = np.clip(ndh, 0, 1) ** 40
    spec2 = np.clip(ndh, 0, 1) ** 6
    ao = 1 - 0.38 * np.exp(-((r - 0.77) / 0.03) ** 2) - 0.22 * sstep(0.94, 1.0, r)
    rng = np.random.default_rng(seed)
    grain = 1 + 0.018 * (rng.standard_normal(r.shape))
    base = np.array(albedo, np.float64)[None, None, :] / 255.0
    shade = (0.50 + 0.62 * diff)[..., None] * ao[..., None] * grain[..., None]
    col = base * shade * np.array([1.04, 1.0, 0.92])[None, None, :]
    col += (0.16 * spec)[..., None] * np.array([1, 0.98, 0.92]) + (0.05 * spec2)[..., None]
    # reflected warm light from the panel on the lower-right outer edge
    col += 0.05 * (np.clip(dx + dy, 0, None) * sstep(0.80, 1.0, r))[..., None] * np.array([1.0, 0.8, 0.5])
    col = np.clip(col, 0, 1)
    alpha = np.clip((1.0 - r) * R / (1.2 * SS), 0, 1)
    rgba = np.dstack([col, alpha])
    im = Image.fromarray((rgba * 255).astype(np.uint8), 'RGBA')
    out = int(2 * Rk + 8)
    return im.resize((out, out), Image.LANCZOS)

if __name__ == '__main__':
    # test sheet on a dark scratched panel
    panel = Image.new('RGB', (700, 330), (34, 34, 36))
    rng = np.random.default_rng(1)
    n = (rng.standard_normal((330, 700)) * 4).astype(int)
    arr = np.array(panel).astype(int) + n[..., None]
    panel = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8))
    for cx, Rk in ((140, 58), (340, 41), (520, 36)):
        cap = render_cap(Rk)
        sh = Image.new('RGBA', panel.size, (0, 0, 0, 0)); d = ImageDraw.Draw(sh)
        d.ellipse((cx - Rk + 5, 165 - Rk + 8, cx + Rk + 5, 165 + Rk + 8), fill=(0, 0, 0, 170))
        sh = sh.filter(ImageFilter.GaussianBlur(Rk * 0.14))
        base = panel.convert('RGBA'); base.alpha_composite(sh)
        base.alpha_composite(cap, (cx - cap.width // 2, 165 - cap.height // 2))
        panel = base.convert('RGB')
    panel.resize((1400, 660), Image.LANCZOS).save('knob_test.png')
