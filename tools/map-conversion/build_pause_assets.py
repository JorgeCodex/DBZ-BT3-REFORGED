"""Convert the pause concept's images to the game's 64x64 sprite packets."""
import subprocess
import sys
import tempfile
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parents[2]
art = root / 'assets/ui/pause'
output = root / 'external/local-only/DBZBT3R'
earth = root / 'assets/maps/earth/earth-adventure-map.unk'
if not earth.exists():
    earth = output / 'earth-adventure-map.unk'
converter = root / 'tools/map-conversion/build_raditz_marker.py'
canvas = Image.new('RGBA', (512, 448))
pixels = canvas.load()
for y in range(448):
    t = y / 447
    for x in range(512):
        shade = 0.62 + 0.38 * x / 511
        pixels[x, y] = (int((51-5*t)*shade), int((185-90*t)*shade), int((189-8*t)*shade), 255)
goku = Image.open(art / 'goku.png').convert('RGBA')
goku.thumbnail((220, 380), Image.Resampling.LANCZOS)
canvas.alpha_composite(goku, (300, 448-goku.height))
draw = ImageDraw.Draw(canvas)
font = ImageFont.truetype('C:/Windows/Fonts/arialbd.ttf', 35)
draw.text((30, 30), 'PAUSA', font=font, fill='#ffad27', stroke_width=2, stroke_fill='#6f260a')
draw.line((30, 75, 238, 75), fill='#ffe283', width=2)
with tempfile.TemporaryDirectory() as temp:
    def packet(im, name):
        source = Path(temp) / 'tile.png'
        im.save(source)
        subprocess.run([sys.executable, str(converter), str(source), str(earth), str(output / name)], check=True, stdout=subprocess.DEVNULL)
    portrait = Image.new('RGBA', (192, 320))
    image = Image.open(art / 'goku.png').convert('RGBA')
    image.thumbnail((192, 320), Image.Resampling.LANCZOS)
    portrait.alpha_composite(image, ((192-image.width)//2, 320-image.height))
    for i in range(15):
        x, y = i % 3 * 64, i // 3 * 64
        packet(portrait.crop((x, y, x+64, y+64)), f'pause-goku-{i:02}.bin')
    for kind in ('normal', 'selected'):
        bar = Image.open(art / f'bar-{kind}.png').convert('RGBA').resize((256, 64), Image.Resampling.LANCZOS)
        for i in range(4):
            packet(bar.crop((i*64, 0, i*64+64, 64)), f'pause-{kind}-{i:02}.bin')
    sheet = Image.open(art / 'icon-sheet.png').convert('RGBA')
    for i, y in enumerate((144, 384, 0, 288)):
        frames = 4 if i == 2 else 5
        for frame in range(frames):
            x = frame * 48
            icon = sheet.crop((x, y, x+48, y+48))
            packet(icon, f'pause-icon-{i}-{frame}.bin')
    labels = ('Reanudar', 'Elegir misión', 'Seleccionar saga', 'Menú principal')
    for state in ('normal', 'selected'):
        for row, text in enumerate(labels):
            tile = Image.new('RGBA', (256, 64))
            d = ImageDraw.Draw(tile)
            size = 28
            f = ImageFont.truetype('C:/Windows/Fonts/arialbd.ttf', size)
            while d.textbbox((0, 0), text, font=f, stroke_width=2)[2] > 248:
                size -= 1
                f = ImageFont.truetype('C:/Windows/Fonts/arialbd.ttf', size)
            border = '#4c164e' if state == 'normal' else '#4a1c18'
            fill = '#f4dff5' if state == 'normal' else '#ffca79'
            d.text((5, 14), text, font=f, fill=fill, stroke_width=3, stroke_fill=border)
            tile.save(art / f'text-{state}-{row}.png')
            for i in range(4):
                packet(tile.crop((i*64, 0, i*64+64, 64)), f'pause-text-{state}-{row}-{i}.bin')
print('Pause sprite packets written to', output)
