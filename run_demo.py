"""Offline gait demo — same math as GaitController.h; exports PNG frames."""
import math
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

OUT = Path(r"C:\iot\robotdog\demo_frames")
OUT.mkdir(exist_ok=True)

NEUTRAL = dict(flHip=90, flKnee=110, frHip=90, frKnee=110,
               blHip=90, blKnee=110, brHip=90, brKnee=110)

def hip(base, s, amp): return base + s * amp
def knee(base, s, amp): return base - (s * amp * 0.9 if s > 0 else 0)

def step_gait(phase, amp):
    s0 = math.sin(phase)
    s1 = math.sin(phase + math.pi)
    return dict(
        flHip=hip(NEUTRAL["flHip"], s0, amp), flKnee=knee(NEUTRAL["flKnee"], s0, amp),
        brHip=hip(NEUTRAL["brHip"], s0, amp), brKnee=knee(NEUTRAL["brKnee"], s0, amp),
        frHip=hip(NEUTRAL["frHip"], s1, amp), frKnee=knee(NEUTRAL["frKnee"], s1, amp),
        blHip=hip(NEUTRAL["blHip"], s1, amp), blKnee=knee(NEUTRAL["blKnee"], s1, amp),
    )

def draw_leg(draw, x, y, hip_a, knee_a, color, label, mirror=False):
    hip_r = math.radians((hip_a - 90) * (-1 if mirror else 1))
    knee_r = math.radians((knee_a - 110) * 0.8)
    L1, L2 = 54, 52
    x1 = x + math.sin(hip_r) * L1
    y1 = y + math.cos(hip_r) * L1
    x2 = x1 + math.sin(hip_r + knee_r) * L2
    y2 = y1 + math.cos(hip_r + knee_r) * L2
    draw.line([(x, y), (x1, y1), (x2, y2)], fill=color, width=7)
    r = 6
    for px, py, rr in ((x, y, 6), (x1, y1, 5), (x2, y2, 4)):
        draw.ellipse([px-rr, py-rr, px+rr, py+rr], fill=color)
    draw.text((x - 22, y - 18), label, fill=(138, 160, 181))

def render(pose, title, path, obstacle=False, pitch=0):
    img = Image.new("RGB", (840, 520), (15, 20, 25))
    d = ImageDraw.Draw(img)
    d.rectangle([0, 400, 840, 520], fill=(30, 42, 54))
    d.line([(40, 400), (800, 400)], fill=(44, 58, 72), width=2)
    body_y = 210
    d.rounded_rectangle([250, body_y, 590, body_y + 70], radius=16, fill=(42, 61, 80))
    d.text((300, body_y + 18), "ESP32-S3 · PCA9685 · MPU6050", fill=(61, 214, 198))
    d.text((560, body_y + 48), "FRONT →", fill=(138, 160, 181))
    d.rectangle([575, body_y + 8, 611, body_y + 30], fill=(138, 160, 181))
    d.text((572, body_y - 14), "SRF04", fill=(255, 180, 84))
    draw_leg(d, 280, body_y + 70, pose["blHip"], pose["blKnee"], (110, 231, 183), "BL CH4/5")
    draw_leg(d, 360, body_y + 70, pose["flHip"], pose["flKnee"], (251, 146, 60), "FL CH0/1")
    draw_leg(d, 480, body_y + 70, pose["frHip"], pose["frKnee"], (96, 165, 250), "FR CH2/3", True)
    draw_leg(d, 560, body_y + 70, pose["brHip"], pose["brKnee"], (192, 132, 252), "BR CH6/7", True)
    lines = [
        f"FL hip {pose['flHip']:.0f}  knee {pose['flKnee']:.0f}",
        f"FR hip {pose['frHip']:.0f}  knee {pose['frKnee']:.0f}",
        f"BL hip {pose['blHip']:.0f}  knee {pose['blKnee']:.0f}",
        f"BR hip {pose['brHip']:.0f}  knee {pose['brKnee']:.0f}",
        f"pitch={pitch}",
    ]
    for i, t in enumerate(lines):
        d.text((40, 30 + i * 18), t, fill=(232, 238, 244))
    d.text((40, 140), title, fill=(61, 214, 198))
    if obstacle:
        d.rectangle([700, 300, 718, 400], fill=(255, 180, 84))
        d.text((660, 280), "vat can <18cm", fill=(255, 180, 84))
    img.save(path)
    print("wrote", path)

# Simulate states matching firmware
render(dict(**NEUTRAL), "STATE: Stand (neutral)", OUT / "01_stand.png")
sit = dict(flHip=90, flKnee=135, frHip=90, frKnee=135, blHip=90, blKnee=155, brHip=90, brKnee=155)
render(sit, "STATE: Sit", OUT / "02_sit.png")

phase = 0.0
for i in range(8):
    phase += 0.25
    pose = step_gait(phase, 22)
    render(pose, f"STATE: Walk  phase={phase:.2f}", OUT / f"03_walk_{i:02d}.png")

phase = 0.0
for i in range(4):
    phase += 0.25 * 1.4
    pose = step_gait(phase, 30)
    render(pose, f"STATE: Run  phase={phase:.2f}", OUT / f"04_run_{i:02d}.png")

# obstacle while walking
pose = step_gait(1.2, 22)
render(pose, "SRF04 obstacle -> Stop", OUT / "05_obstacle_stop.png", obstacle=True)

# fall / getup
getup0 = dict(flHip=70, flKnee=140, frHip=70, frKnee=140, blHip=70, blKnee=140, brHip=70, brKnee=140)
render(getup0, "MPU6050 fall pitch=62 -> GetUp step0", OUT / "06_getup.png", pitch=62)

# also make a small GIF of walk cycle
frames = []
phase = 0.0
for i in range(16):
    phase += 0.25
    pose = step_gait(phase, 22)
    p = OUT / f"_tmp_{i:02d}.png"
    render(pose, f"WALK DEMO frame {i}", p)
    frames.append(Image.open(p).convert("P", palette=Image.ADAPTIVE))
gif = OUT / "walk_cycle.gif"
frames[0].save(gif, save_all=True, append_images=frames[1:], duration=80, loop=0)
for p in OUT.glob("_tmp_*.png"):
    p.unlink()
print("GIF", gif)
print("DONE frames=", len(list(OUT.glob('*.png'))) + 1)
