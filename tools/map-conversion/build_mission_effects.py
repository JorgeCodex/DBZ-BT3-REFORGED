"""Create Adventure mission beam and fade textures from an existing 64x64 packet."""
import argparse
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("template", type=Path, help="Existing 64x64 BT3 marker .bin")
    parser.add_argument("output", type=Path, help="Directory for the generated .bin files")
    args = parser.parse_args()
    template = args.template.read_bytes()
    if len(template) != 64 + 16512:
        raise ValueError("Expected a 64x64 direct-colour marker packet")
    args.output.mkdir(parents=True, exist_ok=True)
    for name, beam in (("mission-column.bin", True), ("mission-fade.bin", False)):
        data = bytearray(template)
        for y in range(64):
            for x in range(64):
                offset = 64 + 0x60 + (y * 64 + x) * 4
                distance = abs((x - 31.5) / 31.5)
                alpha = round(75 * max(0, 1 - distance) ** 0.7)
                data[offset:offset + 4] = bytes((255, 25, 25, alpha) if beam else (0, 0, 0, 128))
        (args.output / name).write_bytes(data)
        print("Created", args.output / name)


if __name__ == "__main__":
    main()
