#!/usr/bin/env python3
"""Draws the device icons for the controller screens into data/images (SD) and data/images_hd (HD).
Every icon is drawn once on a square canvas in a 48x48 design box and resized to each asset size, so the
HD art (3 x 2.25 pixels per design pixel, which the 16:9 output stretches back) matches the SD art.
usage: tools/make_icons.py   (run from the repo root; needs Pillow)"""
from PIL import Image, ImageDraw
import os

S = 16                      # supersampling
N = 48 * S
INK = (58, 58, 58, 255)     # body outline / dark parts
BODY = (246, 246, 246, 255) # light body
MID = (196, 196, 196, 255)
DARK = (88, 88, 88, 255)

def new():
    im = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    return im, ImageDraw.Draw(im)

def p(v):                   # design units -> canvas pixels
    return v * S

def rrect(d, x0, y0, x1, y1, r, fill=BODY, outline=INK, w=1.6):
    d.rounded_rectangle([p(x0), p(y0), p(x1), p(y1)], radius=p(r), fill=fill, outline=outline, width=int(p(w)))

def ell(d, cx, cy, rx, ry=None, fill=BODY, outline=INK, w=1.4):
    ry = rx if ry is None else ry
    d.ellipse([p(cx - rx), p(cy - ry), p(cx + rx), p(cy + ry)], fill=fill, outline=outline, width=int(p(w)) if outline else 0)

def poly(d, pts, fill=BODY, w=1.6):
    pp = [(p(x), p(y)) for x, y in pts]
    d.polygon(pp, fill=fill)
    d.line(pp + [pp[0], pp[1]], fill=INK, width=int(p(w)), joint="curve")

def dpad(d, cx, cy, s=4.2, t=1.5, fill=INK):
    d.rectangle([p(cx - s), p(cy - t), p(cx + s), p(cy + t)], fill=fill)
    d.rectangle([p(cx - t), p(cy - s), p(cx + t), p(cy + s)], fill=fill)

def joystick():
    im, d = new()
    poly(d, [(8, 44), (40, 44), (36, 31), (12, 31)])
    rrect(d, 6, 31, 42, 44, 3)
    d.line([(p(24), p(31)), (p(24), p(14))], fill=INK, width=int(p(3.6)))
    ell(d, 24, 10, 6.5, fill=INK, outline=INK)
    ell(d, 22, 8, 1.8, fill=(150, 150, 150, 255), outline=None)
    ell(d, 14, 38, 2.4, fill=INK, outline=None)
    ell(d, 33, 38, 2.4, fill=INK, outline=None)
    return im

def keyboard():
    im, d = new()
    rrect(d, 3, 11, 45, 37, 4)
    for r in range(3):
        for c in range(8 if r < 2 else 0):
            x = 7 + c * 4.6
            rrect(d, x, 15 + r * 6.2, x + 3.2, 18.6 + r * 6.2, 0.8, fill=MID, outline=None, w=0)
    rrect(d, 12, 27.4, 36, 30.6, 0.8, fill=MID, outline=None, w=0)
    return im

def mouse():
    im, d = new()
    rrect(d, 14, 4, 34, 44, 10)
    d.line([(p(14.8), p(20)), (p(33.2), p(20))], fill=INK, width=int(p(1.4)))
    d.line([(p(24), p(5)), (p(24), p(20))], fill=INK, width=int(p(1.4)))
    rrect(d, 22.2, 9, 25.8, 15, 1.6, fill=INK, outline=None, w=0)
    return im

def wiimote_at(d, x, y, w=14, h=44):
    rrect(d, x, y, x + w, y + h, 4)
    cx = x + w / 2
    ell(d, cx, y + 6, 2.2, fill=MID)                    # power
    dpad(d, cx, y + 13, 3.2, 1.1)
    ell(d, cx, y + 21, 3.0, fill=MID)                   # A
    ell(d, cx - 2.2, y + 28, 1.3, fill=MID, w=0.8)      # - and home
    ell(d, cx + 2.2, y + 28, 1.3, fill=MID, w=0.8)
    ell(d, cx, y + 31.5, 1.5, fill=MID, w=0.8)
    ell(d, cx - 2.6, y + 36, 1.5, fill=INK, outline=None)   # 1 and 2
    ell(d, cx + 2.6, y + 36, 1.5, fill=INK, outline=None)

def wiimote():
    im, d = new()
    wiimote_at(d, 17, 2)
    return im

def pointer():
    im, d = new()
    wiimote_at(d, 17, 14, 14, 32)
    for i, r in enumerate((6, 11, 16)):                 # the IR cone
        d.arc([p(24 - r), p(14 - r), p(24 + r), p(14 + r)], 215, 325, fill=DARK, width=int(p(1.6)))
    return im

def wiimote_nunchuk():
    im, d = new()
    wiimote_at(d, 5, 2)
    poly(d, [(28, 18), (44, 18), (46, 26), (41, 46), (31, 46), (26, 26)])
    ell(d, 36, 12, 3.4, fill=INK, outline=INK)
    d.line([(p(36), p(15)), (p(36), p(19))], fill=INK, width=int(p(2.4)))
    ell(d, 36, 27, 2.6, fill=MID)
    rrect(d, 31, 33, 41, 37, 2, fill=MID, w=1.0)
    return im

def stick(d, cx, cy, r=3.3):
    ell(d, cx, cy, r, fill=MID)
    ell(d, cx, cy, r * 0.45, fill=INK, outline=None)

def classic():
    im, d = new()
    poly(d, [(4, 16), (44, 16), (46, 32), (41, 40), (33, 32), (15, 32), (7, 40), (2, 32)])
    rrect(d, 3, 15, 45, 20, 3, w=1.4)
    dpad(d, 11, 24, 3.4, 1.1)
    for dx, dy in ((38, 21), (34, 25), (42, 25), (38, 29)):
        ell(d, dx, dy, 1.7, fill=MID, w=0.9)
    stick(d, 17, 31, 3.0)
    stick(d, 31, 31, 3.0)
    ell(d, 24, 24, 1.2, fill=INK, outline=None)
    return im

def wiiu_pro():
    im, d = new()
    rrect(d, 4, 14, 44, 29, 7)
    rrect(d, 3, 24, 13, 42, 5)
    rrect(d, 35, 24, 45, 42, 5)
    rrect(d, 4, 14, 44, 29, 7, outline=None, w=0)
    rrect(d, 4, 14, 44, 29, 7)
    stick(d, 13, 21, 3.4)
    stick(d, 31, 28, 3.4)
    dpad(d, 17, 29, 3.0, 1.0)
    for dx, dy in ((38, 18), (34, 22), (42, 22), (38, 26)):
        ell(d, dx, dy, 1.6, fill=MID, w=0.8)
    ell(d, 24, 19, 1.6, fill=INK, outline=None)
    return im

def gamecube():
    im, d = new()
    rrect(d, 3, 10, 45, 36, 12)
    rrect(d, 17, 34, 31, 44, 5)
    stick(d, 11, 20, 3.8)
    dpad(d, 17, 30, 3.0, 1.0)
    ell(d, 35, 21, 4.4, fill=(140, 140, 140, 255))      # A
    ell(d, 28.5, 27.5, 1.8, fill=MID, w=0.9)            # B
    ell(d, 28.5, 18, 1.8, fill=MID, w=0.9)
    ell(d, 40, 29, 1.8, fill=MID, w=0.9)
    stick(d, 31, 31, 2.4)
    return im

def gamepad():
    im, d = new()
    rrect(d, 2, 9, 46, 39, 9)
    rrect(d, 13, 12, 35, 36, 2, fill=(70, 70, 70, 255), outline=INK, w=1.2)
    stick(d, 7.5, 18, 2.8)
    stick(d, 40.5, 29, 2.8)
    dpad(d, 7.5, 27, 2.4, 0.9)
    for dx, dy in ((40.5, 16), (38, 19), (43, 19), (40.5, 22)):
        ell(d, dx, dy, 1.3, fill=MID, w=0.7)
    return im

ICONS = {"joystick": joystick, "keyboard": keyboard, "mouse": mouse, "wiimote": wiimote, "pointer": pointer,
         "wiimote_nunchuk": wiimote_nunchuk, "classic": classic, "wiiu_pro": wiiu_pro, "gamecube": gamecube, "gamepad": gamepad}

SIZES = {"data/images": (48, 48), "data/images_hd": (144, 108)}   # design 48x48 at the SD and HD asset scales

for name, fn in ICONS.items():
    big = fn()
    for folder, size in SIZES.items():
        os.makedirs(folder, exist_ok=True)
        big.resize(size, Image.LANCZOS).save(os.path.join(folder, "icon_%s.png" % name), optimize=True)
print("wrote %d icons" % len(ICONS))
