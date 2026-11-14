#!/usr/bin/env python3
"""Writes the levels of detail of the ModelLod check (Assets/Models/Lod), with no dependency: one sphere of radius 1
at three finenesses, the same shape a level lighter each time, as a model and its levels would be.
- Sphere0.obj: 48 segments by 24 rings (2,208 triangles), the full detail;
- Sphere1.obj: 16 by 8 (224 triangles);
- Sphere2.obj: 6 by 3 (24 triangles), a coarse ball whose facets a screenshot shows;
- Offset0.obj: Sphere1 moved 40 m along +Z from its pivot, as a model whose geometry stands far from its origin.
Run from anywhere: python3 Tests/GameChecks/Tools/make_lods.py"""

import math
import os

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Assets", "Models", "Lod")
LEVELS = [(48, 24), (16, 8), (6, 3)]
OFFSET = 40.0


def sphere(segments, rings):
    """Positions and normals from pole to pole, and triangles facing out (counter-clockwise seen from outside)"""
    vertices = []
    for ring in range(rings + 1):
        polar = math.pi * ring / rings
        for segment in range(segments + 1):
            azimuth = 2.0 * math.pi * segment / segments
            vertices.append((math.sin(polar) * math.cos(azimuth), math.cos(polar), math.sin(polar) * math.sin(azimuth)))

    triangles = []
    row = segments + 1
    for ring in range(rings):
        for segment in range(segments):
            a = ring * row + segment
            b = a + row
            if ring > 0:
                triangles.append((a, a + 1, b))
            if ring < rings - 1:
                triangles.append((a + 1, b + 1, b))
    return vertices, triangles


def write(path, vertices, triangles, offset=(0.0, 0.0, 0.0)):
    with open(path, "w", newline="\n") as file:
        file.write("# Written by Tests/GameChecks/Tools/make_lods.py\n")
        for x, y, z in vertices:
            file.write(f"v {x + offset[0]:.6f} {y + offset[1]:.6f} {z + offset[2]:.6f}\n")
        for x, y, z in vertices:
            file.write(f"vn {x:.6f} {y:.6f} {z:.6f}\n")
        for a, b, c in triangles:
            file.write(f"f {a + 1}//{a + 1} {b + 1}//{b + 1} {c + 1}//{c + 1}\n")


def main():
    os.makedirs(OUT, exist_ok=True)
    for level, (segments, rings) in enumerate(LEVELS):
        vertices, triangles = sphere(segments, rings)
        write(os.path.join(OUT, f"Sphere{level}.obj"), vertices, triangles)
        print(f"Sphere{level}.obj: {len(triangles)} triangles")

    vertices, triangles = sphere(*LEVELS[1])
    write(os.path.join(OUT, "Offset0.obj"), vertices, triangles, (0.0, 0.0, OFFSET))
    print(f"Offset0.obj: {len(triangles)} triangles, {OFFSET:g} m from its pivot")


if __name__ == "__main__":
    main()
