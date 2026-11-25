"""Helpers of the preview tests: small glTF files and a PNG reader, with only
the Python standard library, so the tests need nothing installed."""

import json
import struct
import zlib
from pathlib import Path

# A box, with the normals, that stands on the floor: x and z from -w/2 to w/2,
# y from 0 to h.
_FACES = [
    # normal, then four corners (counterclockwise seen from outside)
    ((0, 0, 1), [(-1, 0, 1), (1, 0, 1), (1, 1, 1), (-1, 1, 1)]),
    ((0, 0, -1), [(1, 0, -1), (-1, 0, -1), (-1, 1, -1), (1, 1, -1)]),
    ((1, 0, 0), [(1, 0, 1), (1, 0, -1), (1, 1, -1), (1, 1, 1)]),
    ((-1, 0, 0), [(-1, 0, -1), (-1, 0, 1), (-1, 1, 1), (-1, 1, -1)]),
    ((0, 1, 0), [(-1, 1, 1), (1, 1, 1), (1, 1, -1), (-1, 1, -1)]),
    ((0, -1, 0), [(-1, 0, -1), (1, 0, -1), (1, 0, 1), (-1, 0, 1)]),
]


def write_box_glb(path, width, height, color):
    """Writes a .glb with a box of width x height x width meters and a
    material of the given RGB color (0 to 1)."""
    positions = []
    normals = []
    indices = []
    for normal, corners in _FACES:
        base = len(positions)
        for x, y, z in corners:
            positions.append((x * width / 2, y * height, z * width / 2))
            normals.append(normal)
        indices += [base, base + 1, base + 2, base, base + 2, base + 3]
    position_bytes = b"".join(struct.pack("<3f", *p) for p in positions)
    normal_bytes = b"".join(struct.pack("<3f", *n) for n in normals)
    index_bytes = b"".join(struct.pack("<H", i) for i in indices)
    blob = position_bytes + normal_bytes + index_bytes
    while len(blob) % 4:
        blob += b"\0"
    low = [min(p[i] for p in positions) for i in range(3)]
    high = [max(p[i] for p in positions) for i in range(3)]
    document = {
        "asset": {"version": "2.0", "generator": "erethea preview test"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "name": "box"}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 2, "material": 0}]}],
        "materials": [
            {"pbrMetallicRoughness": {"baseColorFactor": [*color, 1.0], "metallicFactor": 0.0, "roughnessFactor": 0.8}}
        ],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": len(positions), "type": "VEC3", "min": low, "max": high},
            {"bufferView": 1, "componentType": 5126, "count": len(normals), "type": "VEC3"},
            {"bufferView": 2, "componentType": 5123, "count": len(indices), "type": "SCALAR"},
        ],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": len(position_bytes), "target": 34962},
            {"buffer": 0, "byteOffset": len(position_bytes), "byteLength": len(normal_bytes), "target": 34962},
            {
                "buffer": 0,
                "byteOffset": len(position_bytes) + len(normal_bytes),
                "byteLength": len(index_bytes),
                "target": 34963,
            },
        ],
        "buffers": [{"byteLength": len(blob)}],
    }
    text = json.dumps(document).encode()
    while len(text) % 4:
        text += b" "
    total = 12 + 8 + len(text) + 8 + len(blob)
    data = struct.pack("<4sII", b"glTF", 2, total)
    data += struct.pack("<I4s", len(text), b"JSON") + text
    data += struct.pack("<I4s", len(blob), b"BIN\0") + blob
    Path(path).write_bytes(data)


def read_png(path):
    """Reads an 8-bit PNG (gray, RGB or RGBA, not interlaced). Returns
    (width, height, channels, rows) with each row a bytearray."""
    data = Path(path).read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG file")
    position = 8
    header = None
    compressed = b""
    while position < len(data):
        length, kind = struct.unpack(">I4s", data[position : position + 8])
        body = data[position + 8 : position + 8 + length]
        position += 12 + length
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT":
            compressed += body
        elif kind == b"IEND":
            break
    width, height, depth, color_type, _, _, interlace = header
    channels = {0: 1, 2: 3, 4: 2, 6: 4}.get(color_type)
    if depth != 8 or channels is None or interlace != 0:
        raise ValueError(f"unsupported PNG (depth {depth}, color type {color_type}, interlace {interlace})")
    raw = zlib.decompress(compressed)
    stride = width * channels
    rows = []
    previous = bytearray(stride)
    offset = 0
    for _ in range(height):
        filter_type = raw[offset]
        line = bytearray(raw[offset + 1 : offset + 1 + stride])
        offset += 1 + stride
        for i in range(stride):
            left = line[i - channels] if i >= channels else 0
            up = previous[i]
            up_left = previous[i - channels] if i >= channels else 0
            if filter_type == 1:
                line[i] = (line[i] + left) & 255
            elif filter_type == 2:
                line[i] = (line[i] + up) & 255
            elif filter_type == 3:
                line[i] = (line[i] + (left + up) // 2) & 255
            elif filter_type == 4:
                estimate = left + up - up_left
                distances = (abs(estimate - left), abs(estimate - up), abs(estimate - up_left))
                best = min(distances)
                predictor = (left, up, up_left)[distances.index(best)]
                line[i] = (line[i] + predictor) & 255
        rows.append(line)
        previous = line
    return width, height, channels, rows


def pixel(image, x, y):
    """The (r, g, b) of a pixel of read_png()'s result."""
    width, _, channels, rows = image
    row = rows[y]
    if channels < 3:
        return (row[x * channels],) * 3
    return tuple(row[x * channels : x * channels + 3])
