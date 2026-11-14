"""Prepare the character selector images as 64x64 native sprite cells."""
import argparse
from pathlib import Path
from PIL import Image, ImageDraw


def write_tiles(image, template, output, prefix):
    image = image.convert("RGBA").resize((256, 256), Image.Resampling.LANCZOS)
    for i in range(16):
        x, y = i % 4 * 64, i // 4 * 64
        rgba = bytearray(image.crop((x, y, x + 64, y + 64)).tobytes())
        for n in range(3, len(rgba), 4):
            rgba[n] = (rgba[n] + 1) // 2
        data = bytearray(template)
        data[64 + 0x60:64 + 0x60 + len(rgba)] = rgba
        (output / f"{prefix}-{i:02}.bin").write_bytes(data)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path, help="Selector image directory")
    parser.add_argument("template", type=Path, help="Existing 64x64 texture packet")
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    packet = args.template.read_bytes()
    if len(packet) != 64 + 16512:
        raise ValueError("Expected a 64x64 native texture packet")
    args.output.mkdir(parents=True, exist_ok=True)
    for source, prefix in (("raditz.png", "selector-raditz"), ("raditz-goku.png", "selector-team")):
        write_tiles(Image.open(args.source / source), packet, args.output, prefix)
    highlight = Image.new("RGBA", (64, 64))
    draw = ImageDraw.Draw(highlight)
    draw.line([(32, 2), (62, 32), (32, 62), (2, 32), (32, 2)], fill=(255, 213, 0, 255), width=4, joint="curve")
    rgba = bytearray(highlight.tobytes())
    for n in range(3, len(rgba), 4):
        rgba[n] = (rgba[n] + 1) // 2
    data = bytearray(packet)
    data[64 + 0x60:64 + 0x60 + len(rgba)] = rgba
    (args.output / "selector-highlight.bin").write_bytes(data)
    print("Prepared 32 selector texture cells and highlight")


if __name__ == "__main__":
    main()
