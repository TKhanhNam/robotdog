from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

try:
    font = ImageFont.truetype("arial.ttf", 28)
    font_s = ImageFont.truetype("arial.ttf", 20)
    font_t = ImageFont.truetype("arialbd.ttf", 34)
except Exception:
    font = ImageFont.load_default()
    font_s = font
    font_t = font

out_dir = Path(r"C:\iot\robotdog\huong-dan")
out_dir.mkdir(exist_ok=True)

src = Image.open(r"C:\iot\robotdog\khung_video\frame_05.jpg").convert("RGB")
guide = Image.new("RGB", (1600, 900), (18, 22, 28))
photo = src.resize((920, 518))
guide.paste(photo, (40, 120))
d = ImageDraw.Draw(guide)

d.text((40, 30), "KHUNG CUA BAN (tu video 7570.mp4) — VI TRI LAP LINH KIEN", fill=(46, 212, 196), font=font_t)
d.text((40, 75), "8-DOF: moi chan = 1 servo hong + 1 servo goi | Than trang in 3D", fill=(180, 196, 210), font=font_s)

panel_x = 980
items = [
    ("1. Tren nap than", "ESP32-S3 + PCA9685 (chong len)"),
    ("2. Trong long than", "2x18650 + XL4015 (trong tam thap)"),
    ("3. Giua than, phang", "MPU6050 (X ve phia dau)"),
    ("4. Tam truoc", "SRF04 (mat / mui)"),
    ("5. Gan dau / co", "INMP441 mic"),
    ("6. Duoi bung / duoi", "MAX98357A + loa 3W"),
    ("7. 4 goc than", "Servo HONG CH0/2/4/6"),
    ("8. Khop chan", "Servo GOI CH1/3/5/7"),
]
y = 130
for title, desc in items:
    d.rounded_rectangle([panel_x, y, 1560, y + 70], radius=10, outline=(46, 212, 196), width=2)
    d.text((panel_x + 14, y + 10), title, fill=(46, 212, 196), font=font_s)
    d.text((panel_x + 14, y + 36), desc, fill=(230, 236, 242), font=font_s)
    y += 84

d.text((40, 660), "MAP SERVO (nhin tu tren xuong, dau phia truoc):", fill=(255, 180, 84), font=font)
map_lines = [
    "FL (truoc trai): HONG CH0 + GOI CH1",
    "FR (truoc phai): HONG CH2 + GOI CH3",
    "BL (sau trai):   HONG CH4 + GOI CH5",
    "BR (sau phai):   HONG CH6 + GOI CH7",
]
for i, t in enumerate(map_lines):
    d.text((40, 700 + i * 32), t, fill=(230, 236, 242), font=font_s)

d.text(
    (40, 840),
    "Luu y: video la mau tham khao — ban chua lap xong. Lap theo thu tu ben duoi.",
    fill=(140, 160, 180),
    font=font_s,
)

out = out_dir / "khung-cua-ban-vi-tri.png"
guide.save(out, quality=95)
print("wrote", out)

leg = Image.open(r"C:\iot\robotdog\khung_video\frame_03.jpg").convert("RGB").resize((1280, 720))
g2 = Image.new("RGB", (1280, 820), (18, 22, 28))
g2.paste(leg, (0, 0))
d2 = ImageDraw.Draw(g2)
d2.rectangle([0, 720, 1280, 820], fill=(24, 30, 38))
d2.text((30, 20), "CHI TIET CHAN: HONG (gan than) + GOI (giua dui-ong)", fill=(46, 212, 196), font=font)
d2.text(
    (30, 735),
    "1) Bat oc servo vao o nhua  2) Bat horn dung goc trung lap  3) Luon day vao trong than",
    fill=(230, 236, 242),
    font=font_s,
)
d2.text(
    (30, 770),
    "Servo hong: quay truoc-sau | Servo goi: gap-duoi chan | Chan nhon cham dat",
    fill=(180, 196, 210),
    font=font_s,
)
out2 = out_dir / "khung-cua-ban-chi-tiet-chan.png"
g2.save(out2, quality=95)
print("wrote", out2)
