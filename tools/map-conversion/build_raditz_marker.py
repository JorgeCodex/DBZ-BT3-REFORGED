import argparse
import struct
from pathlib import Path
from PIL import Image


def main():
    parser = argparse.ArgumentParser(description='Prepare the Raditz Adventure marker for BT3.')
    parser.add_argument('icon', type=Path)
    parser.add_argument('map', type=Path, help='Current converted Earth map (texture packet template)')
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    raw = args.map.read_bytes()
    start, end = struct.unpack_from('<II', raw, 4)
    model = raw[start:end]
    ptr = lambda offset: struct.unpack_from('<I', model, offset)[0] * 4
    textures = ptr(0x54)
    table = ptr(textures + 4)
    entry = model[table + 85 * 64:table + 86 * 64]
    pixels, clut, size, _ = struct.unpack_from('<4I', entry)
    if size != 16512 or clut != 0:
        raise ValueError('Expected the Earth map 64x64 direct-colour texture template')
    packet = bytearray(model[pixels * 4:pixels * 4 + size])
    if len(packet) != size:
        raise ValueError('Truncated texture packet')
    image = Image.open(args.icon).convert('RGBA')
    image.thumbnail((64, 64), Image.Resampling.LANCZOS)
    canvas = Image.new('RGBA', (64, 64))
    canvas.alpha_composite(image, ((64 - image.width) // 2, (64 - image.height) // 2))
    rgba = bytearray(canvas.tobytes())
    for i in range(3, len(rgba), 4):
        rgba[i] = (rgba[i] + 1) // 2  # GS alpha: 0..128.
    packet[0x60:0x60 + len(rgba)] = rgba
    args.output.write_bytes(entry + packet)
    print(f'Wrote {args.output}')


if __name__ == '__main__':
    main()
