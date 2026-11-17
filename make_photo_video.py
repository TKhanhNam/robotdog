from pathlib import Path
import numpy as np
from PIL import Image
import imageio.v2 as imageio

src = Path(r"C:\iot\robotdog\huong-dan\anh-tung-buoc")
names = [
    "buoc-01-khung-trang.png",
    "buoc-02-servo-hong.png",
    "buoc-03-servo-goi.png",
    "buoc-04-cam-jack-servo.png",
    "buoc-05-8-servo-pca.png",
    "buoc-06-nguon-5v.png",
    "buoc-07-pca-nguon-i2c.png",
    "buoc-08-esp32-i2c.png",
    "buoc-09-srf04.png",
    "buoc-10-micro.png",
    "buoc-11-loa.png",
    "buoc-12-hoan-thien.png",
]
frames = []
for n in names:
    im = Image.open(src / n).convert("RGB").resize((1280, 720), Image.Resampling.LANCZOS)
    frames.append(np.array(im))

out = Path(r"C:\iot\robotdog\huong-dan\ANH-HUONG-DAN-LAP.mp4")
imageio.mimsave(str(out), frames, fps=1 / 3.5, codec="libx264", quality=7)
print("MP4", out, round(out.stat().st_size / 1e6, 2), "MB")
