"""Tao GIF huong dan lap (video ngan) tu bo anh buoc."""
from pathlib import Path
from PIL import Image

SRC = Path(r"C:\iot\robotdog\huong-dan")
OUT_GIF = SRC / "huong-dan-lap-rap.gif"
OUT_MP4_HINT = SRC / "README-VIDEO.txt"

names = [
    "huong-dan-00-bia.png",
    "huong-dan-01-khung.png",
    "huong-dan-02-servo.png",
    "huong-dan-03-nguon.png",
    "huong-dan-04-board.png",
    "huong-dan-05-tin-hieu.png",
    "huong-dan-06-checklist.png",
    "huong-dan-07-nap-code.png",
]

frames = []
for n in names:
    im = Image.open(SRC / n).convert("RGB")
    im = im.resize((960, 540), Image.Resampling.LANCZOS)
    frames.append(im)

# duration per frame ms
frames[0].save(
    OUT_GIF,
    save_all=True,
    append_images=frames[1:],
    duration=1800,
    loop=0,
    optimize=True,
)
print("GIF:", OUT_GIF, "size_MB=", round(OUT_GIF.stat().st_size / 1e6, 2))

OUT_MP4_HINT.write_text(
    "Cach xem huong dan:\n"
    "1) Mo C:\\iot\\robotdog\\huong-dan-video.html bang trinh duyet (slideshow nhu video).\n"
    "2) Mo huong-dan-lap-rap.gif (tu chay lap).\n"
    "3) Anh tung buoc: huong-dan-00 ... huong-dan-07.\n",
    encoding="utf-8",
)
print("DONE")
