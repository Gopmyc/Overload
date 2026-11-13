#!/usr/bin/env python3
"""Writes the height map and the colour map of the Terrain check (Assets/Textures/Terrain), with no dependency:
- Hills.png: 129 x 129 grey pixels of 16 bits, rolling hills (a pixel per sample, its rows along +Z from the top);
- HillsColor.png: 129 x 129 RGB pixels of 8 bits, grass low, earth on the slopes, snow on the tops.
Run from anywhere: python3 Tests/GameChecks/Tools/make_heightmap.py"""

import math
import os
import struct
import zlib

SIZE = 129
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Assets", "Textures", "Terrain")


def height(x, z):
    """From 0 to 1: two waves across the field, and a ridge along its diagonal"""
    value = 0.5
    value += 0.22 * math.sin(x * 2.0 * math.pi / 64.0) * math.cos(z * 2.0 * math.pi / 80.0)
    value += 0.12 * math.sin((x + z) * 2.0 * math.pi / 48.0)
    return min(max(value, 0.0), 1.0)


def colour(value):
    """Grass, then earth, then snow, blended by height"""
    stops = [(0.0, (70, 120, 55)), (0.55, (95, 140, 70)), (0.7, (120, 100, 75)), (0.85, (235, 235, 240)), (1.0, (250, 250, 255))]
    for (a, ca), (b, cb) in zip(stops, stops[1:]):
        if value <= b:
            t = (value - a) / (b - a)
            return tuple(round(ca[i] + t * (cb[i] - ca[i])) for i in range(3))
    return stops[-1][1]


def png(path, width, height_, depth, colour_type, rows):
    """A PNG of filter-free rows, compressed by zlib"""
    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    raw = b"".join(b"\x00" + row for row in rows)
    header = struct.pack(">IIBBBBB", width, height_, depth, colour_type, 0, 0, 0)
    with open(path, "wb") as file:
        file.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def main():
    os.makedirs(OUT, exist_ok=True)
    grey = [b"".join(struct.pack(">H", round(height(x, z) * 65535)) for x in range(SIZE)) for z in range(SIZE)]
    rgb = [b"".join(bytes(colour(height(x, z))) for x in range(SIZE)) for z in range(SIZE)]
    png(os.path.join(OUT, "Hills.png"), SIZE, SIZE, 16, 0, grey)
    png(os.path.join(OUT, "HillsColor.png"), SIZE, SIZE, 8, 2, rgb)


if __name__ == "__main__":
    main()
