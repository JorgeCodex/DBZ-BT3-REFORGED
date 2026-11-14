"""Convert scouter tutorial banners to native 64x64 sprite cells."""
import argparse
from pathlib import Path
from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument("source", type=Path)
parser.add_argument("template", type=Path)
parser.add_argument("output", type=Path)
a = parser.parse_args()
template = a.template.read_bytes()
if len(template) != 64 + 16512:
    raise ValueError("Expected a native 64x64 texture packet")
a.output.mkdir(parents=True, exist_ok=True)
for state in ("pending", "completed"):
    image = Image.open(a.source / (state + ".png")).convert("RGBA")
    bounds = image.getchannel("A").getbbox()
    if not bounds:
        raise ValueError("Empty banner: " + state)
    image = image.crop(bounds).resize((512, 64), Image.Resampling.LANCZOS)
    for i in range(8):
        tile = bytearray(image.crop((i * 64, 0, (i + 1) * 64, 64)).tobytes())
        for j in range(3, len(tile), 4):
            tile[j] = (tile[j] + 1) // 2
        data = bytearray(template)
        data[64 + 0x60:64 + 0x60 + len(tile)] = tile
        (a.output / f"scouter-prompt-{state}-{i:02}.bin").write_bytes(data)
print("Prepared 16 banner cells")
