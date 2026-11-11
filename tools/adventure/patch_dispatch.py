"""Route the Raditz Adventure dispatcher independently of the native battle call.

Apply only to the known previously patched v0.1.39.6 executable; preserve a backup.
"""
import argparse
import hashlib
import struct
from pathlib import Path

EXPECTED_SHA256 = "d25f9d858154474b742cd5f30498ec84d215ea1fb32df884a03cf0353c299e8a"
CAVE, HOOK, BATTLE = 0x31E1160, 0x31E1078, 0x2700

def patch(path: Path) -> None:
    data = bytearray(path.read_bytes())
    if hashlib.sha256(data).hexdigest() != EXPECTED_SHA256:
        raise ValueError("Ejecutable no reconocido; no se modifico")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    optional = pe + 24
    sections = optional + struct.unpack_from("<H", data, pe + 20)[0]
    count = struct.unpack_from("<H", data, pe + 6)[0]
    def offset(rva):
        for i in range(count):
            h = sections + i * 40
            _, va, raw_size, raw = struct.unpack_from("<IIII", data, h + 8)
            if va <= rva < va + raw_size:
                return raw + rva - va
        raise ValueError(f"RVA no encontrado: {rva:#x}")
    def replace(rva, old, new):
        pos = offset(rva)
        if data[pos:pos + len(old)] != old:
            raise ValueError(f"Instrucción inesperada en {rva:#x}")
        data[pos:pos + len(new)] = new
    # call export; restore stack; only tail-call Battle_Main on result 1.
    payload = bytes.fromhex("ff d0 48 83 c4 28 85 c0 74 07 31 c9 e9")
    payload += struct.pack("<i", BATTLE - (CAVE + 17)) + b"\xc3"
    replace(CAVE, bytes(len(payload)), payload)
    replace(HOOK, bytes.fromhex("ff d0 48 83 c4"), b"\xe9" + struct.pack("<i", CAVE - HOOK - 5))
    for rva in (0x31E1059, 0x31E1072):
        pos = offset(rva)
        if data[pos:pos+2] != b"\x0f\x84":
            raise ValueError(f"Salto inesperado en {rva:#x}")
        struct.pack_into("<i", data, pos + 2, CAVE + 2 - (rva + 6))
    for i in range(count):
        h = sections + i * 40
        if data[h:h+8].rstrip(b"\0") == b".raditz":
            struct.pack_into("<I", data, h + 8, 0x180)
            break
    else:
        raise ValueError("Sección de Adventure no encontrada")
    path.write_bytes(data)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    patch(parser.parse_args().executable)
