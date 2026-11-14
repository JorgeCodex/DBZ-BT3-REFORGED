"""Generate three independent yellow scouter triangles as native sprites."""
import argparse
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter

parser = argparse.ArgumentParser()
parser.add_argument("template", type=Path)
parser.add_argument("output", type=Path)
a = parser.parse_args()
template = a.template.read_bytes()
if len(template) != 64 + 16512:
    raise ValueError("Expected a 64x64 native sprite packet")
a.output.mkdir(parents=True, exist_ok=True)
for direction, points in (("right", [(18,12),(49,32),(18,52)]), ("left", [(46,12),(15,32),(46,52)]), ("up", [(12,46),(32,15),(52,46)])):
    mask = Image.new("L", (256, 256))
    ImageDraw.Draw(mask).polygon([(x*4,y*4) for x,y in points], fill=255)
    mask = mask.resize((64,64), Image.Resampling.LANCZOS)
    glow = mask.filter(ImageFilter.GaussianBlur(3)).point(lambda v: v*2//3)
    img = Image.new("RGBA", (64,64), (255, 185, 0, 0)); img.putalpha(glow)
    face = Image.new("RGBA", (64,64), (255, 225, 58, 0)); face.putalpha(mask)
    img = Image.alpha_composite(img, face)
    rgba = bytearray(img.tobytes())
    for i in range(3, len(rgba), 4): rgba[i] = (rgba[i]+1)//2
    packet = bytearray(template); packet[64+0x60:64+0x60+len(rgba)] = rgba
    (a.output / ("scouter-reticle-" + direction + ".bin")).write_bytes(packet)
print("Prepared yellow scouter triangles")
