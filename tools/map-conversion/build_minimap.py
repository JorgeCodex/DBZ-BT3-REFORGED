"""Build the transparent Earth minimap and player arrows as BT3 texture packets."""
import argparse
import subprocess
import sys
import tempfile
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter

parser = argparse.ArgumentParser()
parser.add_argument('map', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
converter = root / 'tools/map-conversion/build_raditz_marker.py'
image = Image.open(root / 'assets/ui/earth-minimap.png').convert('RGBA')
image = image.resize((256, 256), Image.Resampling.LANCZOS)
# Add a dark blue rim and a soft blue halo around land, leaving the ocean transparent.
alpha = image.getchannel("A")
expanded = alpha.filter(ImageFilter.MaxFilter(7))
halo_alpha = expanded.filter(ImageFilter.GaussianBlur(5)).point(lambda a: a * 3 // 5)
halo = Image.new("RGBA", image.size, (0, 98, 230, 0))
halo.putalpha(halo_alpha)
rim = Image.new("RGBA", image.size, (2, 17, 60, 0))
rim.putalpha(expanded.point(lambda a: a * 4 // 5))
image = Image.alpha_composite(Image.alpha_composite(halo, rim), image)
args.output.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory() as tmp:
    temp = Path(tmp) / 'tile.png'
    def build(img, name):
        img.save(temp)
        subprocess.run([sys.executable, str(converter), str(temp), str(args.map), str(args.output / name)], check=True)
    for i in range(16):
        x, y = (i % 4) * 64, (i // 4) * 64
        build(image.crop((x,y,x+64,y+64)), f'minimap-{i:02d}.bin')
    arrow = Image.new('RGBA', (64,64), (0,0,0,0))
    ImageDraw.Draw(arrow).polygon([(32,3),(56,57),(32,46),(8,57)], fill='white', outline='black', width=4)
    for i in range(32):
        build(arrow.rotate(-360*i/32, resample=Image.Resampling.BICUBIC), f'minimap-arrow-{i}.bin')
