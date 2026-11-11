"""Export the static draw groups of the converted Dragon Adventure Earth package."""

import argparse
from collections import defaultdict
from io import BytesIO
import json
from pathlib import Path
import struct

from PIL import Image

from convert_earth import geometry_chains, pointer, vertex_batches, word


GROUPS = (
    "Cielo y fondo", "Mar lejano", "Terreno, edificios y vegetación",
    "Capas translúcidas A", "Capas translúcidas B", "Capas translúcidas C",
)

BLOCK32 = (
    (0, 1, 4, 5, 16, 17, 20, 21), (2, 3, 6, 7, 18, 19, 22, 23),
    (8, 9, 12, 13, 24, 25, 28, 29), (10, 11, 14, 15, 26, 27, 30, 31),
)
BLOCK16 = tuple(tuple(BLOCK32[x][y] for x in range(4)) for y in range(8))


def gs_offset_32(x, y, width):
    block = ((y & ~31) * max(width, 1) + ((x >> 1) & ~31)
             + BLOCK32[(y >> 3) & 3][(x >> 3) & 7])
    column = ((y & 6) * 8) + ((x & 6) * 2) + ((y & 1) * 2) + (x & 1)
    return block * 256 + column * 4


def gs_index_address(x, y, width, four_bit):
    column = (y >> 2) & 3
    x = x ^ (4 if (column ^ (y >> 1)) & 1 else 0)
    low = ((x & 1) << 2) | ((y & 1) << 3) | ((x & 6) << 3) | ((x & 8) >> 2) | ((y & 2) >> 1)
    if four_bit:
        block = (y >> 7) * 32 * (width >> 1) + ((x >> 7) * 32)
        block += BLOCK16[(y >> 4) & 7][(x >> 5) & 3]
        nibble = block * 512 + column * 128 + ((low & ~1) << 1) + (low & 1)
        nibble += ((x & 16) >> 2) - ((x & 8) >> 2)
        return nibble // 2, (nibble & 1) * 4
    block = (y >> 6) * 32 * (width >> 1) + ((x >> 7) * 32)
    block += BLOCK32[(y >> 4) & 3][(x >> 4) & 7]
    return block * 256 + column * 64 + low, 0


def upload_32(data, packet, size, width):
    # A GIF A+D upload header followed by a single IMAGE payload.
    transfer = struct.unpack_from("<Q", data, packet + 0x30)[0]
    w, h = transfer & 0xFFF, (transfer >> 32) & 0xFFF
    payload = packet + 0x60
    if not w or not h or w * h * 4 > size - 0x60:
        raise ValueError("Invalid texture transfer")
    vram = bytearray(max(8192, w * h * 8))
    for y in range(h):
        for x in range(w):
            offset = gs_offset_32(x, y, width)
            if offset + 4 > len(vram):
                vram.extend(bytes(offset + 4 - len(vram)))
            src = payload + (y * w + x) * 4
            vram[offset:offset + 4] = data[src:src + 4]
    return vram


def texture_image(data, entry):
    pix, pal, pix_size, pal_size, _, _, pix_blt, pal_blt = struct.unpack_from("<8I", data, entry)
    tex0, = struct.unpack_from("<Q", data, entry + 0x30)
    width = (tex0 >> 14) & 63
    psm = (tex0 >> 20) & 63
    w, h = 1 << ((tex0 >> 26) & 15), 1 << ((tex0 >> 30) & 15)
    if pix_blt >> 8 or pal_blt >> 8 or psm not in (0, 0x13, 0x14):
        raise ValueError(f"Unsupported Earth texture format {psm:#x}")
    pixels = upload_32(data, pix * 4, pix_size, pix_blt & 63)
    palette = upload_32(data, pal * 4, pal_size, pal_blt & 63) if pal else None
    rgba = bytearray()
    for y in range(h):
        for x in range(w):
            if palette is None:
                offset = gs_offset_32(x, y, width)
                color = pixels[offset:offset + 4]
            else:
                offset, shift = gs_index_address(x, y, width, psm == 0x14)
                index = pixels[offset] >> shift
                if psm == 0x14:
                    index &= 15
                    offset = gs_offset_32(index & 7, index >> 3, 1)
                else:
                    # CSM1 swaps bit 3 with bit 4 for 256-color palettes.
                    index = (index & 0xE7) | ((index & 8) << 1) | ((index & 16) >> 1)
                    offset = gs_offset_32(index & 15, index >> 4, 1)
                color = palette[offset:offset + 4]
            if len(color) != 4:
                raise ValueError("Texture address outside the transfer")
            rgba.extend((color[0], color[1], color[2], min(255, color[3] * 2)))
    return Image.frombytes("RGBA", (w, h), bytes(rgba))


def unpack_geometry(data):
    by_group = {}
    for group, name in enumerate(GROUPS):
        materials = defaultdict(list)
        for texture, chain in geometry_chains(data, group):
            for offset, vertices in vertex_batches(data, chain):
                batch = tuple(struct.iter_unpack("<12f", data[offset:offset + vertices * 48]))
                materials[texture].append(batch)
        by_group[name] = materials
    return by_group


class Glb:
    def __init__(self):
        self.data = bytearray()
        self.doc = dict(
            asset={"version": "2.0", "generator": "Dragon Adventure Earth exporter"},
            extensionsUsed=["KHR_materials_unlit"],
            scene=0, scenes=[{"name": "Tierra — Dragon Adventure", "nodes": []}],
            nodes=[], meshes=[], images=[], textures=[], materials=[], bufferViews=[], accessors=[],
            samplers=[{"wrapS": 10497, "wrapT": 10497, "magFilter": 9729, "minFilter": 9987}],
        )

    def add(self, kind, item):
        entries = self.doc[kind]
        index = len(entries)
        entries.append(item)
        return index

    def view(self, data, target=None):
        self.data.extend(bytes(-len(self.data) % 4))
        view = {"buffer": 0, "byteOffset": len(self.data), "byteLength": len(data)}
        if target is not None:
            view["target"] = target
        self.data.extend(data)
        return self.add("bufferViews", view)

    def accessor(self, data, component, count, kind, target, **extra):
        return self.add("accessors", dict(
            bufferView=self.view(data, target), componentType=component, count=count, type=kind, **extra
        ))

    def add_texture(self, image, index):
        out = BytesIO()
        image.save(out, format="PNG")
        image_index = self.add("images", dict(
            name=f"Textura BT2 {index:02}", mimeType="image/png", bufferView=self.view(out.getvalue())
        ))
        return self.add("textures", {"sampler": 0, "source": image_index})

    def write(self, output):
        self.data.extend(bytes(-len(self.data) % 4))
        self.doc["buffers"] = [{"byteLength": len(self.data)}]
        metadata = json.dumps(self.doc, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        metadata += b" " * (-len(metadata) % 4)
        with output.open("wb") as out:
            out.write(struct.pack("<4sII", b"glTF", 2, 28 + len(metadata) + len(self.data)))
            out.write(struct.pack("<I4s", len(metadata), b"JSON"))
            out.write(metadata)
            out.write(struct.pack("<I4s", len(self.data), b"BIN\0"))
            out.write(self.data)


def geometry_primitive(glb, batches, material):
    vertices = []
    unique = {}
    indices = []
    for batch in batches:
        strip = []
        for vertex in batch:
            x, y, z, _, r, g, b, alpha, u, v, _, _ = vertex
            color = tuple(round(min(1, max(0, c / 128)) ** 2.2 * 255) for c in (r, g, b))
            vertex = (x, -y, -z, u, v, *color, min(255, max(0, int(alpha * 2))))
            index = unique.setdefault(vertex, len(vertices))
            if index == len(vertices):
                vertices.append(vertex)
            strip.append(index)
        for i in range(2, len(strip)):
            a, b, c = strip[i - 2:i + 1]
            pa, pb, pc = (vertices[j] for j in (a, b, c))
            ab = tuple(pb[k] - pa[k] for k in range(3))
            ac = tuple(pc[k] - pa[k] for k in range(3))
            if all(abs(ab[k] * ac[(k + 1) % 3] - ac[k] * ab[(k + 1) % 3]) < 1e-8 for k in range(3)):
                continue
            indices.extend((b, a, c) if i & 1 else (a, b, c))
    if not indices:
        return None, 0
    position_data = b"".join(struct.pack("<3f", *v[:3]) for v in vertices)
    uv_data = b"".join(struct.pack("<2f", *v[3:5]) for v in vertices)
    color_data = bytes(c for v in vertices for c in v[5:])
    pos = glb.accessor(
        position_data, 5126, len(vertices), "VEC3", 34962,
        min=[min(v[k] for v in vertices) for k in range(3)],
        max=[max(v[k] for v in vertices) for k in range(3)],
    )
    uv = glb.accessor(uv_data, 5126, len(vertices), "VEC2", 34962)
    color = glb.accessor(color_data, 5121, len(vertices), "VEC4", 34962, normalized=True)
    ind = glb.accessor(struct.pack(f"<{len(indices)}I", *indices), 5125, len(indices), "SCALAR", 34963)
    return {"attributes": {"POSITION": pos, "TEXCOORD_0": uv, "COLOR_0": color},
            "indices": ind, "material": material}, len(indices) // 3


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    package = args.source.read_bytes()
    start, end = struct.unpack_from("<II", package, 4)
    data = package[start:end]
    groups = unpack_geometry(data)
    texture_file = pointer(data, 0x54)
    texture_count = word(data, texture_file)
    texture_table = pointer(data, texture_file + 4)
    glb = Glb()
    textures = {}
    materials = {}
    triangle_count = 0
    for group, (name, mappings) in enumerate(groups.items()):
        primitives = []
        for tex, batches in mappings.items():
            if not 0 <= tex < texture_count:
                raise ValueError(f"Invalid texture index {tex}")
            if tex not in textures:
                image = texture_image(data, texture_table + tex * 64)
                textures[tex] = glb.add_texture(image, tex), image.getchannel("A").getextrema()[0]
            texture, min_alpha = textures[tex]
            mode = "BLEND" if group >= 3 else "MASK" if min_alpha < 255 else "OPAQUE"
            key = (tex, mode)
            if key not in materials:
                props = dict(
                    name=f"BT2 {tex:02} {mode}", doubleSided=True,
                    extensions={"KHR_materials_unlit": {}}, alphaMode=mode,
                    pbrMetallicRoughness={"baseColorTexture": {"index": texture}, "metallicFactor": 0},
                )
                if mode == "MASK":
                    props["alphaCutoff"] = 0.5
                materials[key] = glb.add("materials", props)
            primitive, count = geometry_primitive(glb, batches, materials[key])
            if primitive is not None:
                primitives.append(primitive)
                triangle_count += count
        if primitives:
            mesh = glb.add("meshes", dict(name=name, primitives=primitives))
            node = glb.add("nodes", dict(name=name, mesh=mesh))
            glb.doc["scenes"][0]["nodes"].append(node)
    glb.write(args.output)
    print(f"{triangle_count} triangles, {len(textures)} embedded textures, {len(glb.doc['meshes'])} mesh groups")


if __name__ == "__main__":
    main()
