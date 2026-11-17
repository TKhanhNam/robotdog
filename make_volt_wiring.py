# -*- coding: utf-8 -*-
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

OUT = Path(r"C:\iot\robotdog\huong-dan")
OUT.mkdir(parents=True, exist_ok=True)
W, H = 1400, 900
BG = (11, 15, 20)
PANEL = (22, 30, 40)
ACC = (46, 196, 182)
TEXT = (236, 242, 248)
MUTED = (155, 172, 188)
YEL = (250, 204, 21)
RED = (239, 68, 68)
GRN = (74, 222, 128)
ORG = (251, 146, 60)
WHT = (255, 255, 255)

try:
    FT = ImageFont.truetype("arialbd.ttf", 32)
    F = ImageFont.truetype("arial.ttf", 22)
    FS = ImageFont.truetype("arial.ttf", 18)
    FB = ImageFont.truetype("arialbd.ttf", 20)
except Exception:
    FT = F = FS = FB = ImageFont.load_default()

im = Image.new("RGB", (W, H), BG)
d = ImageDraw.Draw(im)
d.rectangle([0, 0, W, 70], fill=(18, 26, 34))
d.text((24, 16), "BUOC 1 — LAP DAY DO DIEN AP (chia ap)", fill=ACC, font=FT)
d.text((24, H - 36), "ESP32 chi cam USB. KHONG noi IN+ / OUT+ thang vao GPIO.", fill=YEL, font=FS)


def box(xy, outline=ACC, fill=PANEL):
    d.rounded_rectangle(xy, radius=12, fill=fill, outline=outline, width=3)


# XL4015
box([40, 100, 430, 430], outline=ORG)
d.text((60, 115), "XL4015 (ha ap)", fill=ORG, font=FT)
d.text((60, 160), "4 oc vit / 4 jack", fill=MUTED, font=FS)
terms = [
    (180, "IN+", "day PIN +", RED),
    (230, "IN-", "day PIN -  = GND", (200, 200, 200)),
    (280, "OUT+", "day 5V ra", ORG),
    (330, "OUT-", "GND ra", (200, 200, 200)),
]
for y, name, note, col in terms:
    d.rounded_rectangle([60, y, 150, y + 36], radius=6, fill=col)
    d.text((70, y + 6), name, fill=(10, 10, 10) if col != (200, 200, 200) else (20, 20, 20), font=FB)
    d.text((165, y + 6), note, fill=TEXT, font=F)

# ESP32
box([970, 100, 1360, 430], outline=ACC)
d.text((990, 115), "ESP32-S3  USB →", fill=ACC, font=FT)
d.text((990, 160), "Hang TREN, USB ben phai:", fill=MUTED, font=FS)
pins = [("GND", 200), ("TX", 245), ("RX", 290), ("1", 335), ("2", 380)]
for name, y in pins:
    col = (200, 200, 200) if name == "GND" else (YEL if name in ("1", "2") else (80, 90, 100))
    d.rectangle([1000, y, 1048, y + 32], fill=col, outline=WHT)
    d.text((1058, y + 4), "GPIO " + name if name.isdigit() else name, fill=TEXT, font=F)
d.text((990, 430 - 40), "Chi dung GND, chan 1, chan 2", fill=YEL, font=FS)

# Divider IN
box([470, 100, 930, 250], outline=RED)
d.text((490, 110), "DO PIN  —  3 diem gap nhau", fill=RED, font=FB)
d.text((490, 150), "IN+  --20k-- * -- GPIO 1", fill=TEXT, font=F)
d.text((630, 185), "|", fill=TEXT, font=F)
d.text((622, 210), "10k", fill=TEXT, font=F)
d.text((630, 235), "GND", fill=MUTED, font=FS)

# Divider OUT
box([470, 270, 930, 430], outline=ORG)
d.text((490, 280), "DO 5V  —  3 diem gap nhau", fill=ORG, font=FB)
d.text((490, 320), "OUT+ --20k-- * -- GPIO 2", fill=TEXT, font=F)
d.text((640, 355), "|", fill=TEXT, font=F)
d.text((632, 380), "10k", fill=TEXT, font=F)
d.text((640, 405), "GND", fill=MUTED, font=FS)

# Steps
box([40, 460, 1360, 840], outline=(80, 90, 100))
steps = [
    "A. Noi GND truoc:  XL4015 IN-  va  OUT-  va  ESP32 GND  (1 day den, chung tat ca).",
    "B. Lay 1 dien tro 20k + 1 dien tro 10k. Xoan 1 dau 20k voi 1 dau 10k = diem * (giua).",
    "C. Dau con lai cua 20k  →  XL4015 IN+     |     dau con lai cua 10k  →  GND.",
    "D. Diem * (cho xoan)  →  ESP32 chan 1.  Day la do PIN, khong phai 5V ESP32.",
    "E. Lam y het 1 bo 20k+10k nua:  20k → OUT+  ,  10k → GND  ,  diem * → chan 2.",
    "F. Cam USB ESP32. Bat pin XL4015. KHONG cam day 5V vao chan 5V cua ESP32 luc do.",
]
for i, t in enumerate(steps):
    d.text((60, 480 + i * 55), t, fill=TEXT, font=F)

p = OUT / "buoc1-chia-ap.png"
im.save(p, quality=95)
print(p)
