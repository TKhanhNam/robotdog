# -*- coding: utf-8 -*-
"""Tao video huong dan cam day chi tiet cho Robot Dog ESP32-S3."""
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont
import imageio.v2 as imageio

OUT = Path(r"C:\iot\robotdog\huong-dan\day")
OUT.mkdir(parents=True, exist_ok=True)
W, H = 1280, 720
BG = (14, 18, 24)
PANEL = (24, 32, 42)
ACC = (46, 212, 196)
WARN = (255, 180, 84)
TEXT = (236, 242, 248)
MUTED = (150, 168, 186)
RED = (239, 68, 68)
GRN = (52, 211, 153)
BLU = (96, 165, 250)
YEL = (250, 204, 21)
ORG = (251, 146, 60)
PUR = (192, 132, 252)
WHT = (255, 255, 255)


def fonts():
    try:
        return (
            ImageFont.truetype("arialbd.ttf", 36),
            ImageFont.truetype("arial.ttf", 26),
            ImageFont.truetype("arial.ttf", 20),
            ImageFont.truetype("arialbd.ttf", 22),
        )
    except Exception:
        f = ImageFont.load_default()
        return f, f, f, f


FT, F, FS, FB = fonts()


def new_slide(title, subtitle=""):
    im = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, W, 78], fill=(20, 28, 36))
    d.text((28, 18), title, fill=ACC, font=FT)
    if subtitle:
        d.text((28, 88), subtitle, fill=MUTED, font=FS)
    d.rectangle([0, H - 42, W, H], fill=(20, 28, 36))
    d.text((28, H - 32), "Robot Dog ESP32-S3 | Cam day theo config.h | GND chung bat buoc", fill=MUTED, font=FS)
    return im, d


def box(d, xy, fill=PANEL, outline=ACC, w=2):
    d.rounded_rectangle(xy, radius=12, fill=fill, outline=outline, width=w)


def wire(d, a, b, color, width=4):
    d.line([a, b], fill=color, width=width)


def chip(d, x, y, w, h, name, pins=None, fill=(35, 48, 62)):
    box(d, [x, y, x + w, y + h], fill=fill, outline=ACC)
    d.text((x + 12, y + 10), name, fill=ACC, font=FB)
    if pins:
        for i, (side, label, col) in enumerate(pins):
            yy = y + 40 + i * 26
            if side == "L":
                d.ellipse([x - 8, yy, x + 4, yy + 12], fill=col)
                d.text((x + 12, yy - 2), label, fill=TEXT, font=FS)
            else:
                d.ellipse([x + w - 4, yy, x + w + 8, yy + 12], fill=col)
                d.text((x + 12, yy - 2), label, fill=TEXT, font=FS)


def save(im, name):
    path = OUT / name
    im.save(path, quality=95)
    print("slide", path.name)
    return path


slides = []

# ----- 00 cover -----
im, d = new_slide("VIDEO HUONG DAN CAM DAY CHI TIET", "8x MG90S + PCA9685 + ESP32-S3 + cam bien + am thanh")
box(d, [80, 140, 1200, 620], fill=(22, 30, 40))
steps = [
    "1. Nguon pin 18650 → XL4015 → 5V",
    "2. Cap 5V / 3.3V / GND cho tung module",
    "3. Cam 8 servo vao PCA9685 CH0..CH7",
    "4. I2C: ESP32 ↔ PCA9685 + MPU6050",
    "5. SRF04, INMP441, MAX98357A + loa",
    "6. Checklist truoc khi bat",
]
for i, t in enumerate(steps):
    d.text((140, 180 + i * 60), t, fill=TEXT, font=F)
slides.append(save(im, "d00_bia.jpg"))

# ----- 01 parts -----
im, d = new_slide("BUOC 0 — NHAN DIEN LINH KIEN TRUOC KHI CAM", "Chuan bi day duc-cai / cai-cai 40P")
parts = [
    (80, 140, "ESP32-S3", "Nao robot"),
    (360, 140, "PCA9685", "Driver 8 servo"),
    (640, 140, "XL4015", "Ha ap xuong 5V"),
    (920, 140, "2x18650", "Pin noi tiep"),
    (80, 340, "8x MG90S", "Dong co chan"),
    (360, 340, "MPU6050", "Phat hien nga"),
    (640, 340, "SRF04", "Sieu am"),
    (920, 340, "INMP441", "Micro"),
    (360, 520, "MAX98357A", "Ampli"),
    (640, 520, "Loa 3W", "Loa mini"),
]
for x, y, name, desc in parts:
    box(d, [x, y, x + 240, y + 120])
    d.text((x + 16, y + 24), name, fill=ACC, font=FB)
    d.text((x + 16, y + 64), desc, fill=MUTED, font=FS)
slides.append(save(im, "d01_linh_kien.jpg"))

# ----- 02 battery -----
im, d = new_slide("BUOC 1 — NOI PIN 18650 VAO XL4015", "Chi NOITIEP tren de pin. DO 5.00V truoc khi cam board!")
box(d, [80, 150, 420, 380], fill=(40, 30, 30), outline=RED)
d.text((110, 170), "DE 2x18650", fill=WARN, font=FB)
d.text((110, 220), "PIN1 + ──┐", fill=TEXT, font=F)
d.text((110, 260), "PIN1 - ──┼── noi tiep", fill=TEXT, font=F)
d.text((110, 300), "PIN2 + ──┘", fill=TEXT, font=F)
d.text((110, 340), "PIN2 -", fill=TEXT, font=F)
d.text((110, 400), "Tong ~7.4V (day 8.4V)", fill=WARN, font=FS)

wire(d, (420, 265), (560, 265), RED, 6)
wire(d, (420, 320), (560, 320), WHT, 6)
d.text((470, 230), "+ 7.4V", fill=RED, font=FS)
d.text((470, 330), "GND", fill=WHT, font=FS)

box(d, [560, 150, 980, 420], fill=(30, 50, 35), outline=GRN)
d.text((590, 170), "XL4015 (ha ap)", fill=GRN, font=FB)
d.text((590, 230), "IN+  ← tu pin +", fill=TEXT, font=F)
d.text((590, 280), "IN-  ← tu pin - / GND", fill=TEXT, font=F)
d.text((590, 340), "OUT+ = 5.00V  (chinh trimmer)", fill=WARN, font=F)
d.text((590, 390), "OUT- = GND", fill=TEXT, font=F)

box(d, [80, 460, 1200, 640], fill=(50, 25, 25), outline=RED)
d.text((110, 490), "CANH BAO", fill=RED, font=FB)
d.text((110, 540), "1) Chinh XL4015 = 5.00V bang dong ho TRUOC khi cam ESP32/servo", fill=TEXT, font=F)
d.text((110, 590), "2) CAM dua 7.4V thang vao ESP32 hoac MG90S", fill=TEXT, font=F)
slides.append(save(im, "d02_pin_xl4015.jpg"))

# ----- 03 5V bus -----
im, d = new_slide("BUOC 2 — CHIA NGUON 5V VA 3.3V", "Do + do = 5V | Den = GND | Xanh = 3.3V tu ESP32")
# 5V rail
d.rectangle([100, 160, 1180, 200], fill=RED)
d.text((110, 168), "+5V BUS (tu XL4015 OUT+)", fill=WHT, font=FB)
d.rectangle([100, 230, 1180, 270], fill=(40, 40, 40))
d.text((110, 238), "GND BUS (chung tat ca)", fill=WHT, font=FB)

loads5 = [
    (120, 320, "PCA9685\nchan V+", "Servo power"),
    (380, 320, "ESP32-S3\nchan 5V/VIN", "Board"),
    (640, 320, "MAX98357A\nchan VIN", "Ampli"),
    (900, 320, "SRF04\nchan VCC", "Sieu am"),
]
for x, y, name, note in loads5:
    box(d, [x, y, x + 220, y + 140], outline=RED)
    d.text((x + 14, y + 20), name, fill=TEXT, font=FS)
    d.text((x + 14, y + 100), note, fill=MUTED, font=FS)
    wire(d, (x + 110, 200), (x + 110, y), RED, 3)

box(d, [120, 500, 1160, 640], outline=BLU)
d.text((140, 520), "3.3V tu chan 3V3 cua ESP32 →", fill=BLU, font=FB)
d.text((140, 570), "PCA9685 VCC (logic)   |   MPU6050 VCC   |   INMP441 VDD", fill=TEXT, font=F)
d.text((140, 610), "Khong dung 5V cho cac chan nay", fill=WARN, font=FS)
slides.append(save(im, "d03_chia_5v_33v.jpg"))

# ----- 04 PCA9685 power + layout -----
im, d = new_slide("BUOC 3 — CAM NGUON VAO PCA9685", "V+ = 5V servo | VCC = 3.3V logic | GND chung")
box(d, [100, 150, 900, 520], fill=(28, 40, 32), outline=GRN)
d.text((130, 170), "PCA9685 (nhin tu tren)", fill=GRN, font=FB)
# terminal block left
d.rectangle([130, 230, 280, 470], fill=(50, 50, 50), outline=WHT)
d.text((145, 250), "V+  ← 5V", fill=RED, font=F)
d.text((145, 310), "GND ← GND", fill=WHT, font=F)
d.text((145, 370), "SCL ← GPIO9", fill=YEL, font=F)
d.text((145, 430), "SDA ← GPIO8", fill=YEL, font=F)
# channels
for i in range(8):
    x = 320 + i * 70
    d.rectangle([x, 250, x + 55, 450], fill=(45, 55, 70), outline=ACC)
    d.text((x + 8, 260), f"CH{i}", fill=ACC, font=FS)
    d.rectangle([x + 8, 300, x + 47, 315], fill=(80, 50, 30))  # PWM
    d.rectangle([x + 8, 330, x + 47, 345], fill=RED)  # V+
    d.rectangle([x + 8, 360, x + 47, 375], fill=(30, 30, 30))  # GND
    d.text((x + 6, 400), "PWM", fill=ORG, font=FS)
    d.text((x + 10, 418), "5V", fill=RED, font=FS)
    d.text((x + 6, 436), "GND", fill=MUTED, font=FS)

box(d, [940, 150, 1220, 520], outline=WARN)
d.text((960, 170), "THU TU CAM", fill=WARN, font=FB)
d.text((960, 230), "1. GND", fill=TEXT, font=F)
d.text((960, 280), "2. V+ = 5V", fill=TEXT, font=F)
d.text((960, 330), "3. VCC = 3.3V", fill=TEXT, font=F)
d.text((960, 380), "4. SDA/SCL", fill=TEXT, font=F)
d.text((960, 430), "5. Roi moi servo", fill=TEXT, font=F)
d.text((120, 560), "Moi hang CH: tren=PWM (cam/vang), giua=5V (do), duoi=GND (nau/den)", fill=MUTED, font=FS)
slides.append(save(im, "d04_pca9685_nguon.jpg"))

# ----- 05 servo colors -----
im, d = new_slide("BUOC 4 — MAU DAY SERVO MG90S", "Cam dung 3 chan tren moi CH cua PCA9685")
# big servo connector
box(d, [120, 160, 600, 560])
d.text((160, 190), "Jack 3 pin MG90S", fill=ACC, font=FB)
d.rectangle([200, 260, 520, 320], fill=ORG)
d.text((220, 275), "CAM / VANG  →  PWM (tin hieu)", fill=(20, 20, 20), font=F)
d.rectangle([200, 350, 520, 410], fill=RED)
d.text((220, 365), "DO           →  V+ / 5V", fill=WHT, font=F)
d.rectangle([200, 440, 520, 500], fill=(40, 40, 40))
d.text((220, 455), "NAU / DEN   →  GND", fill=WHT, font=F)

box(d, [660, 160, 1180, 560], outline=WARN)
d.text((690, 190), "CACH CAM VAO PCA9685", fill=WARN, font=FB)
d.text((690, 260), "• Hang 3 lo moi CH: PWM | 5V | GND", fill=TEXT, font=F)
d.text((690, 320), "• Cam thang hang, khong lech 1 chan", fill=TEXT, font=F)
d.text((690, 380), "• Neu servo rung / nong: kiem tra", fill=TEXT, font=F)
d.text((690, 420), "  dao PWM voi 5V", fill=TEXT, font=F)
d.text((690, 480), "• Chi cam 1 jack / 1 CH", fill=TEXT, font=F)
slides.append(save(im, "d05_mau_day_servo.jpg"))

# ----- 06 map all servos -----
im, d = new_slide("BUOC 5 — MAP 8 SERVO VAO CH0..CH7", "Nhin tu tren xuong, DAU cho phia TRUOC")
# body
box(d, [430, 200, 850, 420], fill=(40, 55, 70))
d.text((560, 280), "THAN CHO", fill=ACC, font=FT)
d.text((580, 340), "↑ DAU", fill=WARN, font=F)

legs = [
    (160, 150, "FL HONG", "CH0", ORG),
    (160, 250, "FL GOI", "CH1", ORG),
    (920, 150, "FR HONG", "CH2", BLU),
    (920, 250, "FR GOI", "CH3", BLU),
    (160, 430, "BL HONG", "CH4", GRN),
    (160, 530, "BL GOI", "CH5", GRN),
    (920, 430, "BR HONG", "CH6", PUR),
    (920, 530, "BR GOI", "CH7", PUR),
]
for x, y, name, ch, col in legs:
    box(d, [x, y, x + 220, y + 70], outline=col)
    d.text((x + 16, y + 12), name, fill=col, font=FB)
    d.text((x + 16, y + 40), f"→ PCA9685 {ch}", fill=TEXT, font=FS)

d.text((400, 480), "HONG = gan goc than | GOI = khop giua dui-ong", fill=MUTED, font=FS)
d.text((400, 520), "Luon day vao long than, keo len noc toi PCA9685", fill=MUTED, font=FS)
slides.append(save(im, "d06_map_8_servo.jpg"))

# ----- 07 I2C -----
im, d = new_slide("BUOC 6 — CAM I2C: ESP32 ↔ PCA9685 + MPU6050", "Chung 1 bus: SDA GPIO8, SCL GPIO9")
chip(d, 80, 180, 280, 320, "ESP32-S3", [
    ("R", "GPIO8 = SDA", YEL),
    ("R", "GPIO9 = SCL", YEL),
    ("R", "3V3", BLU),
    ("R", "GND", WHT),
    ("R", "5V/VIN", RED),
])
chip(d, 520, 160, 300, 200, "PCA9685", [
    ("L", "SDA", YEL),
    ("L", "SCL", YEL),
    ("L", "VCC=3.3V", BLU),
    ("L", "GND", WHT),
])
d.text((540, 300), "V+ = 5V (rieng)", fill=RED, font=FS)
chip(d, 520, 400, 300, 200, "MPU6050", [
    ("L", "SDA", YEL),
    ("L", "SCL", YEL),
    ("L", "VCC=3.3V", BLU),
    ("L", "GND", WHT),
])
# wires
wire(d, (360, 230), (520, 210), YEL, 4)
wire(d, (360, 256), (520, 236), YEL, 4)
wire(d, (360, 282), (520, 262), BLU, 4)
wire(d, (360, 308), (520, 288), WHT, 4)
wire(d, (670, 360), (670, 400), YEL, 3)
wire(d, (700, 360), (700, 400), YEL, 3)
box(d, [900, 180, 1220, 560], outline=ACC)
d.text((920, 200), "BANG NOI", fill=ACC, font=FB)
lines = [
    "ESP32 GPIO8 → SDA",
    "  (PCA9685 + MPU6050)",
    "ESP32 GPIO9 → SCL",
    "  (PCA9685 + MPU6050)",
    "ESP32 3V3 → VCC logic",
    "ESP32 GND → GND",
    "XL4015 5V → PCA9685 V+",
    "",
    "Dia chi I2C:",
    "PCA9685 = 0x40",
    "MPU6050 = 0x68",
]
for i, t in enumerate(lines):
    d.text((920, 250 + i * 26), t, fill=TEXT, font=FS)
slides.append(save(im, "d07_i2c.jpg"))

# ----- 08 SRF04 -----
im, d = new_slide("BUOC 7 — CAM SRF04 (SIEU AM) + CHIA AP ECHO", "ESP32-S3 KHONG chiu 5V — bat buoc chia ap chan ECHO")
chip(d, 80, 180, 260, 280, "SRF04", [
    ("R", "VCC → 5V", RED),
    ("R", "TRIG", ORG),
    ("R", "ECHO (5V!)", WARN),
    ("R", "GND", WHT),
])
chip(d, 860, 180, 300, 300, "ESP32-S3", [
    ("L", "GPIO10 ← TRIG", ORG),
    ("L", "GPIO11 ← ECHO*", WARN),
    ("L", "GND", WHT),
])
wire(d, (340, 260), (860, 240), ORG, 4)
d.text((500, 210), "TRIG → GPIO10", fill=ORG, font=FS)

# divider
box(d, [400, 320, 780, 560], outline=WARN)
d.text((420, 340), "CHIA AP ECHO (bat buoc)", fill=WARN, font=FB)
d.text((420, 400), "ECHO ──[1kΩ]──●── GPIO11", fill=TEXT, font=F)
d.text((560, 440), "│", fill=TEXT, font=F)
d.text((560, 470), "[2kΩ]", fill=TEXT, font=F)
d.text((560, 500), "│", fill=TEXT, font=F)
d.text((560, 530), "GND", fill=TEXT, font=F)
d.text((100, 520), "Gan SRF04 o TAM TRUOC khung (mui cho)", fill=MUTED, font=FS)
slides.append(save(im, "d08_srf04.jpg"))

# ----- 09 INMP441 -----
im, d = new_slide("BUOC 8 — CAM MICRO INMP441 (I2S)", "Gan o co/dau, huong ra ngoai. L/R noi GND = kenh trai")
chip(d, 100, 170, 360, 420, "INMP441", [
    ("R", "VDD → 3.3V", BLU),
    ("R", "GND", WHT),
    ("R", "WS / LRCL", PUR),
    ("R", "SCK / BCLK", PUR),
    ("R", "SD / DOUT", PUR),
    ("R", "L/R → GND", WHT),
])
chip(d, 720, 170, 420, 420, "ESP32-S3", [
    ("L", "3V3", BLU),
    ("L", "GND", WHT),
    ("L", "GPIO4 ← WS", PUR),
    ("L", "GPIO5 ← SCK", PUR),
    ("L", "GPIO6 ← SD", PUR),
])
wire(d, (460, 280), (720, 280), BLU, 3)
wire(d, (460, 306), (720, 306), WHT, 3)
wire(d, (460, 332), (720, 332), PUR, 4)
wire(d, (460, 358), (720, 358), PUR, 4)
wire(d, (460, 384), (720, 384), PUR, 4)
d.text((500, 540), "WS→GPIO4 | SCK→GPIO5 | SD→GPIO6 | L/R→GND | VDD→3.3V", fill=MUTED, font=FS)
slides.append(save(im, "d09_inmp441.jpg"))

# ----- 10 MAX98357A -----
im, d = new_slide("BUOC 9 — CAM MAX98357A + LOA 3W", "Loa khong huong thang vao mic (tranh hu)")
chip(d, 80, 170, 380, 400, "MAX98357A", [
    ("R", "VIN → 5V", RED),
    ("R", "GND", WHT),
    ("R", "BCLK", BLU),
    ("R", "LRC", BLU),
    ("R", "DIN", BLU),
    ("R", "+ / - loa", WARN),
])
chip(d, 700, 160, 420, 280, "ESP32-S3", [
    ("L", "GPIO16 → BCLK", BLU),
    ("L", "GPIO17 → LRC", BLU),
    ("L", "GPIO18 → DIN", BLU),
])
box(d, [700, 470, 1120, 600], outline=WARN)
d.text((720, 500), "LOA MINI 3W", fill=WARN, font=FB)
d.text((720, 550), "+ va - noi thang toi MAX98357A", fill=TEXT, font=FS)
wire(d, (460, 300), (700, 220), BLU, 4)
wire(d, (460, 326), (700, 246), BLU, 4)
wire(d, (460, 352), (700, 272), BLU, 4)
d.text((100, 600), "Vi tri: duoi bung / gan duoi khung trang", fill=MUTED, font=FS)
slides.append(save(im, "d10_max98357a.jpg"))

# ----- 11 full pin table -----
im, d = new_slide("BUOC 10 — BANG TONG HOP GPIO (config.h)", "In bang nay ra de cam day ben canh robot")
rows = [
    ("Chuc nang", "ESP32-S3", "Noi toi"),
    ("I2C SDA", "GPIO 8", "PCA9685 SDA + MPU6050 SDA"),
    ("I2C SCL", "GPIO 9", "PCA9685 SCL + MPU6050 SCL"),
    ("SRF04 TRIG", "GPIO 10", "TRIG"),
    ("SRF04 ECHO", "GPIO 11", "qua chia ap 1k+2k"),
    ("Mic WS", "GPIO 4", "INMP441 WS"),
    ("Mic SCK", "GPIO 5", "INMP441 SCK"),
    ("Mic SD", "GPIO 6", "INMP441 SD"),
    ("Spk BCLK", "GPIO 16", "MAX98357A BCLK"),
    ("Spk LRC", "GPIO 17", "MAX98357A LRC"),
    ("Spk DIN", "GPIO 18", "MAX98357A DIN"),
]
for i, (a, b, c) in enumerate(rows):
    y = 120 + i * 42
    fill = (32, 44, 58) if i == 0 else (PANEL if i % 2 == 0 else (28, 36, 46))
    d.rectangle([60, y, 1220, y + 40], fill=fill)
    col = ACC if i == 0 else TEXT
    d.text((80, y + 8), a, fill=col, font=FS)
    d.text((360, y + 8), b, fill=col, font=FS)
    d.text((620, y + 8), c, fill=col, font=FS)
slides.append(save(im, "d11_bang_gpio.jpg"))

# ----- 12 checklist -----
im, d = new_slide("BUOC 11 — CHECKLIST TRUOC KHI BAT NGUON", "Tick tung muc. Sai day = chay board/servo")
checks = [
    "XL4015 do dung 5.00V (chua cam board khi chinh)",
    "Tat ca GND da noi chung",
    "PCA9685: V+=5V, VCC=3.3V, SDA=8, SCL=9",
    "8 servo dung CH0..CH7, dung thu tu PWM-5V-GND",
    "MPU6050: 3.3V, SDA, SCL, nam phang giua than",
    "SRF04 ECHO da qua 1k + 2k vao GPIO11",
    "INMP441: WS4 SCK5 SD6, L/R=GND, VDD=3.3V",
    "MAX98357A: BCLK16 LRC17 DIN18, VIN=5V, loa da cam",
    "Day khong ket khop chan; robot ke vung",
    "USB A-C san sang nap code, Serial 115200",
]
for i, t in enumerate(checks):
    y = 130 + i * 48
    d.rectangle([90, y, 130, y + 32], outline=ACC, width=2)
    d.text((150, y + 4), t, fill=TEXT, font=FS)
slides.append(save(im, "d12_checklist.jpg"))

# ----- 13 nap code -----
im, d = new_slide("BUOC 12 — NAP CODE VA LENH THU", "Sau khi cam day xong")
box(d, [100, 150, 620, 580])
d.text((130, 180), "Arduino IDE", fill=ACC, font=FB)
d.text((130, 240), "Board: ESP32S3 Dev Module", fill=TEXT, font=F)
d.text((130, 290), "USB CDC On Boot: Enabled", fill=TEXT, font=F)
d.text((130, 340), "Mo folder robotdog → Upload", fill=TEXT, font=F)
d.text((130, 400), "Serial Monitor: 115200", fill=TEXT, font=F)
box(d, [680, 150, 1180, 580], outline=WARN)
d.text((710, 180), "LENH THU DAY", fill=WARN, font=FB)
cmds = ["stand", "sit", "walk", "stop", "getup", "chat Xin chao"]
for i, c in enumerate(cmds):
    d.text((710, 250 + i * 45), f"> {c}", fill=GRN, font=F)
d.text((710, 530), "Chan lech → SERVO_DIR / NEUTRAL", fill=MUTED, font=FS)
slides.append(save(im, "d13_nap_code.jpg"))

# Build MP4 + GIF
print("Building MP4...")
frames = []
for p in slides:
    im = Image.open(p).convert("RGB")
    frames.append(np.array(im))

mp4 = Path(r"C:\iot\robotdog\huong-dan\HUONG-DAN-CAM-DAY.mp4")
# 2.5 sec per slide
fps = 1 / 2.5
imageio.mimsave(str(mp4), frames, fps=fps, codec="libx264", quality=7)
print("MP4", mp4, "MB", round(mp4.stat().st_size / 1e6, 2))

# GIF smaller
gif_frames = [Image.fromarray(f).resize((854, 480)) for f in frames]
gif = Path(r"C:\iot\robotdog\huong-dan\HUONG-DAN-CAM-DAY.gif")
gif_frames[0].save(gif, save_all=True, append_images=gif_frames[1:], duration=2200, loop=0, optimize=True)
print("GIF", gif, "MB", round(gif.stat().st_size / 1e6, 2))
print("SLIDES", len(slides))
