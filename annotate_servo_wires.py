from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

src = Path(r"C:\Users\admin\.cursor\projects\c-iot-robotdog\assets\c__Users_admin_AppData_Roaming_Cursor_User_workspaceStorage_5ec4d2e7a4c9881c7d3d87f7ecb0ef24_images_WIN_20260907_14_58_55_Pro-deca9c3a-3a63-4683-9a7b-75a2dc34def2.jpg")
out_dir = Path(r"C:\iot\robotdog\huong-dan\anh-tung-buoc")
out_dir.mkdir(parents=True, exist_ok=True)

photo = Image.open(src).convert("RGB")
# make landscape canvas
W, H = 1600, 900
canvas = Image.new("RGB", (W, H), (16, 20, 26))
# fit photo on left
pw, ph = photo.size
scale = min(820 / pw, 820 / ph)
photo2 = photo.resize((int(pw * scale), int(ph * scale)))
canvas.paste(photo2, (30, (H - photo2.size[1]) // 2))

d = ImageDraw.Draw(canvas)
try:
    ft = ImageFont.truetype("arialbd.ttf", 34)
    f = ImageFont.truetype("arial.ttf", 24)
    fs = ImageFont.truetype("arialbd.ttf", 22)
except Exception:
    ft = f = fs = ImageFont.load_default()

d.text((30, 20), "DAY MG90S CUA BAN  →  CAM VAO PCA9685", fill=(46, 212, 196), font=ft)

# right instruction panel
x0 = 900
d.rounded_rectangle([x0, 80, 1560, 820], radius=16, fill=(28, 36, 46), outline=(46, 212, 196), width=3)
d.text((x0 + 24, 100), "3 mau day = 3 chan tren moi CH", fill=(46, 212, 196), font=fs)

rows = [
    ((250, 204, 21), "VANG", "PWM / tin hieu", "Chan PWM (sat chu PWM, ve giua board)"),
    ((239, 68, 68), "DO", "5V / nguon", "Chan V+ (o giua)"),
    ((20, 20, 20), "DEN", "GND / mass", "Chan GND (sat mep board)"),
]
y = 160
for col, name, meaning, where in rows:
    d.rounded_rectangle([x0 + 24, y, 1536, y + 140], radius=12, fill=(22, 28, 36), outline=col, width=3)
    d.ellipse([x0 + 40, y + 40, x0 + 90, y + 90], fill=col, outline=(255, 255, 255), width=2)
    d.text((x0 + 110, y + 18), name, fill=col, font=ft)
    d.text((x0 + 110, y + 62), meaning, fill=(236, 242, 248), font=f)
    d.text((x0 + 110, y + 96), where, fill=(160, 176, 192), font=f)
    y += 155

d.text((x0 + 24, 640), "Cach cam: giu jack thang hang", fill=(255, 180, 84), font=fs)
d.text((x0 + 24, 680), "VANG doi dien PWM, DO doi dien V+,", fill=(236, 242, 248), font=f)
d.text((x0 + 24, 714), "DEN doi dien GND. Khong lech 1 chan.", fill=(236, 242, 248), font=f)
d.text((x0 + 24, 760), "FL hong = CH0, FL goi = CH1, ... BR goi = CH7", fill=(160, 176, 192), font=f)

out = out_dir / "cam-day-mg90s-cua-ban.png"
canvas.save(out, quality=95)
print("wrote", out)
