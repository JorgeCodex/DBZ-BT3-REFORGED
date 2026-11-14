"""Build the scouter filter and tiled frame for the local game package."""
import argparse
import subprocess
import sys
import tempfile
from pathlib import Path
from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument('map', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
converter = root / 'tools/map-conversion/build_raditz_marker.py'
frame = Image.open(root / 'assets/ui/scouter-frame-bt3.png').convert('RGBA')
if frame.size != (512, 448):
    raise ValueError('The BT3 scouter frame must be a transparent 512x448 PNG')
args.output.mkdir(parents=True, exist_ok=True)
# Remove stale cells from the previous 4x4, low-resolution version.
for stale_tile in args.output.glob('scouter-frame-*.bin'):
    stale_tile.unlink()
with tempfile.TemporaryDirectory() as temp:
    temp = Path(temp)
    def build(image, name):
        path = temp / 'input.png'
        image.save(path)
        subprocess.run([sys.executable, str(converter), str(path), str(args.map), str(args.output / name)], check=True)
    build(Image.new('RGBA', (64, 64), (0x2d, 0xf8, 0x32, 96)), 'scouter-filter.bin')
    for i in range(8 * 7):
        x, y = (i % 8) * 64, (i // 8) * 64
        tile = frame.crop((x, y, x + 64, y + 64))
        if tile.getchannel('A').getbbox() is not None:
            build(tile, f'scouter-frame-{i:02d}.bin')
