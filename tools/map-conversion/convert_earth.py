import argparse
import math
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


def convert_tree(model, scale=1.0):
    original_size = len(model)
    root, = struct.unpack_from("<I", model, 0x50)
    model.extend(bytes(-len(model) % 16))
    visited = set()

    def convert(word_offset, parent, depth):
        if word_offset in visited or not 0 < word_offset * 4 <= original_size - 44:
            raise ValueError("Invalid BT2 tree node")
        visited.add(word_offset)
        _, a, b, c, d, _, x, z, old_depth, count, parts = struct.unpack_from(
            "<6I2f3I", model, word_offset * 4
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
            "<10I3f3I", model, converted * 4, parent,
            children[0], children[1], 0, 0, children[2], children[3], 0, 0,
            0, x * scale, 0, z * scale, depth, count, parts if count else 0,
        )
        return converted

    struct.pack_into("<I", model, 0x50, convert(root, 0, 0))
    return len(visited)


def word(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


def pointer(data, offset):
    return word(data, offset) * 4


def geometry_chains(data, group):
    groups = pointer(data, 0x1C)
    _, count, materials = struct.unpack_from("<HHI", data, groups + group * 8)
    for i in range(count):
        texture, meshes, mesh_offset, animation = struct.unpack_from("<HHII", data, materials * 4 + i * 16)
        if animation:
            raise ValueError("Unexpected animated Earth texture")
        for mesh in range(meshes):
            desc = mesh_offset * 4 + mesh * 16
            if data[desc + 3] != 0:
                raise ValueError("Unexpected Earth mesh program")
            yield texture, pointer(data, desc + 8)


def vertex_batches(data, chain):
    tag = word(data, chain)
    if tag >> 28 != 6:  # DMA RET with an inline VIF stream
        raise ValueError("Unsupported Earth DMA chain")
    offset = chain + 8
    end = chain + 16 + (tag & 0xFFFF) * 16
    gif_tag = 0
    while offset < end:
        command = word(data, offset)
        offset += 4
        opcode = command >> 24 & 127
        if opcode in (0, 0x17):  # NOP and MSCNT
            continue
        if opcode != 0x6C:  # UNPACK V4-32
            raise ValueError(f"Unsupported VIF opcode {opcode:#x}")
        count = (command >> 16 & 255) or 256
        address = command & 1023
        if offset + count * 16 > end:
            raise ValueError("Truncated VIF upload")
        if address == 0 and count == 3:
            gif_tag, = struct.unpack_from("<Q", data, offset + 32)
        elif address == 3:
            vertices = gif_tag & 0x7FFF
            if gif_tag >> 47 & 7 != 4 or vertices * 3 != count:
                raise ValueError("Unexpected stage primitive")
            yield offset, vertices
        offset += count * 16


def scale_floats(data, offset, count, scale):
    fmt = f"<{count}f"
    values = struct.unpack_from(fmt, data, offset)
    struct.pack_into(fmt, data, offset, *(value * scale for value in values))


def scale_collision(data, scale):
    zone_count = word(data, 0x38)
    zones = pointer(data, 0x3C)
    meshes = set()
    for i in range(zone_count):
        zone = zones + i * 0x40
        scale_floats(data, zone + 0x20, 3, scale)  # Center
        scale_floats(data, zone + 0x30, 3, scale)  # Half extents
        if word(data, zone + 0x14):
            raise ValueError("Unexpected destructible Earth collision")
        mesh = pointer(data, zone + 4)
        if mesh:
            meshes.add(mesh)

    for mesh in sorted(meshes):
        nodes, polygons = struct.unpack_from("<II", data, mesh)
        node_table, polygon_table, vertex_table = (pointer(data, mesh + offset) for offset in (0xC, 0x10, 0x14))
        for i in range(nodes):
            scale_floats(data, node_table + i * 0x20, 6, scale)
        vertices = set()
        for i in range(polygons):
            polygon = polygon_table + i * 0x20
            vertices.update(struct.unpack_from("<3I", data, polygon + 4))
            scale_floats(data, polygon + 0x1C, 1, scale)  # Plane distance, not its normal
        for index in vertices:
            scale_floats(data, vertex_table + index * 0x10, 3, scale)


def scale_earth_model(data, scale):
    chains = {chain for group in range(7) for _, chain in geometry_chains(data, group)}
    for chain in sorted(chains):
        for start, vertices in vertex_batches(data, chain):
            for offset in range(start, start + vertices * 48, 48):
                scale_floats(data, offset, 3, scale)  # Position, before color and UV

    part_lists = pointer(data, 0x24)
    for i in range(word(data, 0x20)):
        count, parts = struct.unpack_from("<II", data, part_lists + i * 8)
        for j in range(count):
            scale_floats(data, parts * 4 + j * 0x20 + 0xC, 4, scale)  # Radius and center
    scale_floats(data, 0x4C, 1, scale)  # Culling tree size
    scale_floats(data, pointer(data, 0x40), 3, scale)  # Flight radius, ceiling and floor
    scale_collision(data, scale)


def scale_placements(data, scale):
    for field in (0x28, 0x30, 0x38):  # Player starts, alternate starts, stage placements
        count, table = struct.unpack_from("<II", data, field)
        for offset in range(table, table + count * 0x20, 0x10):
            scale_floats(data, offset, 3, scale)  # Position and look target
    if word(data, 0x10):
        scale_floats(data, word(data, 0x14) + 8, 1, scale)  # Water level


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("earth", type=Path)
    parser.add_argument("base", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--scale", type=float, default=1.0, help="Uniform scale relative to the original BT2 map")
    args = parser.parse_args()
    if not math.isfinite(args.scale) or args.scale <= 0:
        parser.error("--scale must be finite and positive")
    bt2 = unpack_package(args.earth.read_bytes())
    bt3 = unpack_package(args.base.read_bytes())
    if len(bt2) != 18 or len(bt3) != 25 or bt3[1][:4] != b"MAPD":
        raise ValueError("Expected an Earth package and a BT3 stage template")
    bt3[0] = bytearray(bt2[0])
    count = convert_tree(bt3[0], args.scale)
    bt3[1] = bytearray(bt3[1])
    struct.pack_into("<I", bt3[1], 8, 0)
    # The combat template blurs distant geometry. The world map uses clear distance rendering.
    if word(bt3[1], 0x50):
        fog_flags = word(bt3[1], 0x54)
        struct.pack_into("<I", bt3[1], fog_flags, word(bt3[1], fog_flags) & ~1)
    # The battle template's weather particles follow the camera. Earth supplies its own scenery.
    if word(bt3[1], 0x60):
        weather_flags = word(bt3[1], 0x64) + 0x10
        struct.pack_into("<I", bt3[1], weather_flags, word(bt3[1], weather_flags) & ~1)
    if args.scale != 1.0:
        scale_earth_model(bt3[0], args.scale)
        scale_placements(bt3[1], args.scale)
    bt3[6] = b""
    args.output.write_bytes(pack_package(bt3))
    print(f"Converted {count} culling nodes at {args.scale:g}x the original BT2 scale")


if __name__ == "__main__":
    main()
