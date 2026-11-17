# -*- coding: utf-8 -*-
"""Anh huong dan cam day ESP32-S3 (N16R8 DevKit) — tung day, tung chan."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

OUT = Path(r"C:\iot\robotdog\huong-dan\anh-esp32")
OUT.mkdir(parents=True, exist_ok=True)
W, H = 1400, 788  # 16:9-ish
BG = (12, 16, 22)
PANEL = (26, 34, 44)
ACC = (46, 212, 196)
TEXT = (240, 244, 248)
MUTED = (156, 172, 188)
YEL = (250, 204, 21)
GRN = (74, 222, 128)
ORG = (251, 146, 60)
RED = (239, 68, 68)
BLK = (30, 30, 30)
BLU = (96, 165, 250)
WHT = (255, 255, 255)
PUR = (192, 132, 252)

try:
    FT = ImageFont.truetype("arialbd.ttf", 34)
    F = ImageFont.truetype("arial.ttf", 24)
    FS = ImageFont.truetype("arial.ttf", 18)
    FB = ImageFont.truetype("arialbd.ttf", 22)
except Exception:
    FT = F = FS = FB = ImageFont.load_default()


def new(title, sub=""):
    im = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, W, 72], fill=(18, 26, 34))
    d.text((24, 16), title, fill=ACC, font=FT)
    if sub:
        d.text((24, 84), sub, fill=MUTED, font=FS)
    d.rectangle([0, H - 40, W, H], fill=(18, 26, 34))
    d.text((24, H - 30), "ESP32-S3-N16R8 cua ban  |  USB-C ben PHAI  |  Cam day duc-cai 40P", fill=MUTED, font=FS)
    return im, d


def box(d, xy, fill=PANEL, outline=ACC, w=2, r=12):
    d.rounded_rectangle(xy, radius=r, fill=fill, outline=outline, width=w)


def save(im, name):
    p = OUT / name
    im.save(p, quality=95)
    print(name)
    return p


# Official-ish DevKitC-1 when USB is on the RIGHT
# Bottom row antenna->USB
BOT = ["3V3", "RST", "4", "5", "6", "7", "15", "16", "17", "18", "8", "3", "46", "9", "10", "11", "12", "13", "14", "5V", "GND"]
TOP = ["GND", "TX", "RX", "1", "2", "42", "41", "40", "39", "38", "37", "36", "35", "0", "45", "48", "47", "21", "20", "19", "GND"]

HIGH = {
    "8": YEL, "9": GRN, "3V3": ORG, "GND": (200, 200, 200),
    "4": PUR, "5": PUR, "6": PUR,
    "16": BLU, "17": BLU, "18": BLU,
    "10": ORG, "11": RED, "5V": RED,
}


def draw_board(d, x, y, highlight=None, show_all=True):
    """Draw ESP32 pinout USB on the right. highlight = set of pin names."""
    bw, bh = 980, 220
    box(d, [x, y, x + bw, y + bh], fill=(20, 24, 28), outline=(80, 90, 100), w=3, r=16)
    # module
    d.rounded_rectangle([x + 16, y + 50, x + 160, y + 170], radius=8, fill=(70, 78, 86))
    d.text((x + 28, y + 90), "S3-N16R8", fill=WHT, font=FS)
    # usb
    d.rounded_rectangle([x + bw - 70, y + 40, x + bw - 12, y + 95], radius=6, fill=(40, 40, 40), outline=WHT)
    d.text((x + bw - 62, y + 55), "COM", fill=ACC, font=FS)
    d.rounded_rectangle([x + bw - 70, y + 115, x + bw - 12, y + 170], radius=6, fill=(40, 40, 40), outline=WHT)
    d.text((x + bw - 62, y + 130), "OTG", fill=MUTED, font=FS)
    d.text((x + 400, y + 95), "USB →", fill=MUTED, font=FB)

    n = len(BOT)
    gap = 40
    start = x + 175

    def pin(px, py, label, row):
        use = highlight is None or label in highlight or (label == "GND" and "GND" in (highlight or []))
        col = HIGH.get(label, (90, 100, 112))
        if highlight and label not in highlight:
            if not (label == "GND" and "GND" in highlight):
                col = (55, 62, 72)
        d.rectangle([px, py, px + 18, py + 22], fill=col, outline=WHT)
        # label
        fill = TEXT if (not highlight or label in highlight) else (90, 98, 108)
        if row == "top":
            d.text((px - 4, py - 26), label, fill=fill, font=FS)
        else:
            d.text((px - 4, py + 24), label, fill=fill, font=FS)

    for i, lab in enumerate(TOP):
        pin(start + i * gap, y + 8, lab, "top")
    for i, lab in enumerate(BOT):
        pin(start + i * gap, y + bh - 30, lab, "bot")
    return start, gap, y, bh


slides = []

# 00 legend
im, d = new("QUY UOC MAU DAY (nho 4 mau nay)", "Dung day duc-cai. Mot mau = mot chuc nang. Khong doi mau giua chung.")
colors = [
    ((40, 40, 40), WHT, "DEN", "GND", "PCA9685 GND  →  ESP32 GND"),
    (ORG, (20, 20, 20), "CAM", "3.3V", "PCA9685 VCC  →  ESP32 3V3"),
    (YEL, (20, 20, 20), "VANG", "SDA", "PCA9685 SDA  →  ESP32 chan 8"),
    (GRN, (20, 20, 20), "XANH LA", "SCL", "PCA9685 SCL  →  ESP32 chan 9"),
]
for i, (bg, fg, name, fn, path) in enumerate(colors):
    x, y = 60, 140 + i * 130
    box(d, [x, y, 1340, y + 110], fill=(22, 28, 36), outline=bg if bg != (40, 40, 40) else WHT, w=4)
    d.rounded_rectangle([x + 20, y + 25, x + 160, y + 85], radius=10, fill=bg, outline=WHT, width=2)
    d.text((x + 40, y + 42), name, fill=fg, font=FB)
    d.text((x + 190, y + 22), fn, fill=ACC, font=FT)
    d.text((x + 190, y + 62), path, fill=TEXT, font=F)
WARN = (255, 180, 84)
d.text((60, 670), "Neu day PCA9685 da cam san: GIU NGUYEN day, chi cam dau kia vao dung chan ESP32.", fill=WARN, font=FS)
slides.append(save(im, "e00-mau-day.jpg"))

# 01 overview board
im, d = new("BUOC 1 — DAT BOARD DUNG HUONG", "USB-C ve BEN PHAI. Hang chan duoi (gan nut RST/BOOT) la hang se cam 4 day.")
draw_board(d, 80, 200, highlight={"8", "9", "3V3", "GND"})
d.text((80, 450), "Tim 4 chu in tren PCB (hang DUOI):", fill=TEXT, font=F)
d.text((80, 500), "3V3     ...     8     3     46     9     10     ...     5V     GND", fill=ACC, font=FB)
d.text((80, 550), "Chan 8 va 9 CACH nhau 2 chan (3 va 46) — dung nham 8 voi 9.", fill=YEL, font=F)
box(d, [80, 600, 1320, 720], outline=ORG)
d.text((100, 620), "CAM:  VCC PCA → 3V3 ESP    (KHONG cam vao 5V)", fill=ORG, font=F)
d.text((100, 665), "5V ESP32 khong dung cho PCA9685 VCC. Nguon servo da vao oc xanh.", fill=MUTED, font=FS)
slides.append(save(im, "e01-huong-board.jpg"))

# 02 wire by wire GND
im, d = new("BUOC 2 — DAY 1 MAU DEN: GND", "Cam truoc tien. Tat ca GND phai chung.")
draw_board(d, 40, 160, highlight={"GND"})
box(d, [80, 430, 1320, 700], fill=(28, 22, 22), outline=WHT, w=3)
d.text((110, 450), "1) Lay 1 day duc-cai MAU DEN", fill=TEXT, font=F)
d.text((110, 500), "2) Mot dau cam vao ESP32 chan GND (goc phai, sat USB)", fill=TEXT, font=F)
d.text((110, 550), "3) Dau kia cam PCA9685 chan GND (hang 6 chan, THU NHAT tu tren)", fill=TEXT, font=F)
d.text((110, 610), "Kiem tra: day den chi di GND → GND. Khong cham 5V / 3V3.", fill=YEL, font=FB)
slides.append(save(im, "e02-day-den-gnd.jpg"))

# 03 3V3
im, d = new("BUOC 3 — DAY 2 MAU CAM: 3.3V (VCC)", "Day nay cap dien CHO CHIP PCA9685, khong phai cho servo.")
draw_board(d, 40, 160, highlight={"3V3"})
box(d, [80, 430, 1320, 700], fill=(40, 28, 16), outline=ORG, w=3)
d.text((110, 450), "1) Lay 1 day duc-cai MAU CAM (hoac do NHO neu het cam)", fill=TEXT, font=F)
d.text((110, 500), "2) Mot dau → ESP32 chan 3V3 (goc TRAI hang duoi, gan anten)", fill=TEXT, font=F)
d.text((110, 550), "3) Dau kia → PCA9685 chan VCC (hang 6 chan, gan cuoi)", fill=TEXT, font=F)
d.text((110, 610), "SAI: cam vao 5V ESP32. DUNG: chi 3V3.", fill=RED, font=FB)
d.text((110, 655), "PCA9685 chan V+ (sat VCC) DE TRONG — 5V servo da o oc xanh.", fill=MUTED, font=FS)
slides.append(save(im, "e03-day-cam-3v3.jpg"))

# 04 SDA
im, d = new("BUOC 4 — DAY 3 MAU VANG: SDA (GPIO 8)", "SDA = duong du lieu I2C. Code dang dung chan 8.")
draw_board(d, 40, 160, highlight={"8"})
box(d, [80, 430, 1320, 700], fill=(40, 36, 12), outline=YEL, w=3)
d.text((110, 450), "1) Lay 1 day duc-cai MAU VANG", fill=TEXT, font=F)
d.text((110, 500), "2) Mot dau → ESP32 chan in so  8  (hang duoi, sau chan 18)", fill=TEXT, font=F)
d.text((110, 550), "   Thu tu:  ... 18 | 8 | 3 | 46 | 9 | 10 ...", fill=ACC, font=FB)
d.text((110, 600), "3) Dau kia → PCA9685 chan SDA", fill=TEXT, font=F)
d.text((110, 650), "Neu PCA da cam day vang vao SDA: dung chinh day do, cam dau kia vao chan 8.", fill=MUTED, font=FS)
slides.append(save(im, "e04-day-vang-sda.jpg"))

# 05 SCL
im, d = new("BUOC 5 — DAY 4 MAU XANH LA: SCL (GPIO 9)", "SCL = duong nhip I2C. Code dang dung chan 9.")
draw_board(d, 40, 160, highlight={"9"})
box(d, [80, 430, 1320, 700], fill=(16, 36, 24), outline=GRN, w=3)
d.text((110, 450), "1) Lay 1 day duc-cai MAU XANH LA", fill=TEXT, font=F)
d.text((110, 500), "2) Mot dau → ESP32 chan in so  9", fill=TEXT, font=F)
d.text((110, 550), "   Dung nham: 8 roi toi 3, 46, ROI MOI TOI 9.", fill=YEL, font=FB)
d.text((110, 600), "3) Dau kia → PCA9685 chan SCL", fill=TEXT, font=F)
d.text((110, 650), "OE tren PCA9685 khong cam gi. V+ hang 6 chan khong cam gi.", fill=MUTED, font=FS)
slides.append(save(im, "e05-day-xanh-scl.jpg"))

# 06 summary 4 wires
im, d = new("BUOC 6 — KIEM TRA 4 DAY DA DU", "Chi 4 day nay la ESP32 dieu khien duoc 8 servo.")
rows = [
    (BLK, WHT, "DEN", "GND", "GND", "GND (sat USB)"),
    (ORG, BLK, "CAM", "3.3V", "VCC", "3V3 (gan anten)"),
    (YEL, BLK, "VANG", "SDA", "SDA", "chan 8"),
    (GRN, BLK, "XANH", "SCL", "SCL", "chan 9"),
]
d.text((80, 130), "Mau day        Chuc nang      PCA9685         ESP32-S3", fill=MUTED, font=FB)
for i, (bg, fg, name, fn, pca, esp) in enumerate(rows):
    y = 180 + i * 90
    d.rounded_rectangle([80, y, 1320, y + 78], radius=10, fill=(24, 30, 38), outline=WHT if bg == BLK else bg, width=3)
    d.rounded_rectangle([100, y + 18, 230, y + 60], radius=8, fill=bg, outline=WHT)
    d.text((120, y + 28), name, fill=fg, font=FB)
    d.text((260, y + 26), fn, fill=TEXT, font=F)
    d.text((520, y + 26), pca, fill=ACC, font=F)
    d.text((820, y + 26), esp, fill=YEL, font=F)
d.text((80, 560), "Thu tu cam an toan: DEN → CAM → VANG → XANH. USB rut, pin TAT khi cam.", fill=TEXT, font=F)
d.text((80, 610), "Nap code bang cong COM. Serial 115200 go: stand", fill=ACC, font=F)
d.text((80, 660), "Loi 'not found': doi cho day vang va xanh (8 ↔ 9).", fill=ORG, font=FS)
slides.append(save(im, "e06-bang-4-day.jpg"))

# 07 later pins full map
im, d = new("PHU LUC — TOAN BO CHAN ESP32 (lam tiep sau)", "Chua can cam het hom nay. Chi can 4 day PCA truoc.")
draw_board(d, 40, 130, highlight=None)
items = [
    "Mic INMP441:  WS=4 (tim)  SCK=5  SD=6   L/R=GND  VDD=3V3",
    "Loa MAX98357A: BCLK=16  LRC=17  DIN=18  VIN=5V(XL4015)  GND",
    "Sieu am SRF04: TRIG=10  ECHO=11 (qua 1k+2k)  VCC=5V  GND",
    "MPU6050: SDA chung chan 8, SCL chung chan 9, VCC=3V3, GND",
]
for i, t in enumerate(items):
    d.text((60, 400 + i * 50), t, fill=TEXT, font=F)
d.text((60, 620), "5V tren ESP32: co the cap ESP32 tu USB hoac tu XL4015 5V → chan 5V + GND.", fill=MUTED, font=FS)
d.text((60, 660), "Khuyen nghi: USB de nap code; pin 18650 + XL4015 de chay servo.", fill=MUTED, font=FS)
slides.append(save(im, "e07-phu-luc-chan-khac.jpg"))

# 08 checklist
im, d = new("BUOC 7 — CHECKLIST TRUOC KHI BAT", "")
checks = [
    "USB rut, cong tac pin TAT",
    "Day DEN: PCA GND ↔ ESP32 GND",
    "Day CAM: PCA VCC ↔ ESP32 3V3   (khong phai 5V)",
    "Day VANG: PCA SDA ↔ ESP32 8",
    "Day XANH: PCA SCL ↔ ESP32 9",
    "PCA OE trong, PCA V+ (hang 6 chan) trong",
    "Oc xanh PCA da co 5.00V tu XL4015",
    "USB cam cong COM, nap robotdog.ino, go stand",
]
for i, t in enumerate(checks):
    y = 130 + i * 62
    d.rectangle([80, y, 128, y + 42], outline=ACC, width=3)
    d.text((150, y + 8), t, fill=TEXT, font=F)
slides.append(save(im, "e08-checklist.jpg"))

print("slides", len(slides))
