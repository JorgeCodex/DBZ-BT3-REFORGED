import argparse
import struct
from pathlib import Path


def unpack_package(data):
    count, = struct.unpack_from("<I", data)
    offsets = struct.unpack_from(f"<{count + 1}I", data, 4)
    if tuple(sorted(offsets)) != offsets or offsets[-1] > len(data):
        raise ValueError("Invalid package offsets")
    return [data[start:end] for start, end in zip(offsets, offsets[1:])]


def pack_package(parts):
    offsets = [128]
    body = bytearray()
    for part in parts:
        body.extend(part)
        body.extend(bytes(-len(body) % 64))
        offsets.append(len(body) + 128)
    header = struct.pack(f"<{len(offsets) + 1}I", len(parts), *offsets)
    return header.ljust(128, b"\0") + body


def convert_tree(model):
    original_size = len(model)
    root, = struct.unpack_from("<I", model, 0x50)
    model.extend(bytes(-len(model) % 16))
    visited = set()

    def convert(word_offset, parent, depth):
        if word_offset in visited or not 0 < word_offset * 4 <= original_size - 44:
            raise ValueError("Invalid BT2 tree node")
        visited.add(word_offset)
        _, a, b, c, d, _, x, z, old_depth, count, parts = struct.unpack_from(
            "<11I", model, word_offset * 4
        )
        if old_depth != depth or depth > 12 or count > 10000:
            raise ValueError("Invalid BT2 tree depth or part count")
        if count and not 0 < parts * 4 <= original_size - count * 4:
            raise ValueError("Invalid BT2 tree part list")
        converted = len(model) // 4
        model.extend(bytes(64))
        children = [convert(child, converted, depth + 1) if child else 0
                    for child in (a, b, c, d)]
        struct.pack_into(
            "<16I", model, converted * 4, parent,
            children[0], children[1], 0, 0, children[2], children[3], 0, 0,
            0, x, 0, z, depth, count, parts if count else 0,
        )
        return converted

    struct.pack_into("<I", model, 0x50, convert(root, 0, 0))
    return len(visited)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("earth", type=Path)
    parser.add_argument("base", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    bt2 = unpack_package(args.earth.read_bytes())
    bt3 = unpack_package(args.base.read_bytes())
    if len(bt2) != 18 or len(bt3) != 25 or bt3[1][:4] != b"MAPD":
        raise ValueError("Expected an Earth package and a BT3 stage template")
    bt3[0] = bytearray(bt2[0])
    count = convert_tree(bt3[0])
    bt3[1] = bytearray(bt3[1])
    struct.pack_into("<I", bt3[1], 8, 0)
    bt3[6] = b""
    args.output.write_bytes(pack_package(bt3))
    print(f"Converted {count} culling nodes")


if __name__ == "__main__":
    main()
