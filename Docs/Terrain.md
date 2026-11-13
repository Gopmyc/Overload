# Terrain

A terrain is two components on one actor: a `PhysicalHeightField`, which holds the heights and collides, and a
`Terrain`, which draws them. The navigation mesh is built from the height field like any static collider (see
[Navigation](Navigation.md)). In the editor, *Actors > Others > Terrain* makes an actor with both.

## The heights

- **From a height map**: `SetHeightmap("Textures/Hills.png")` (`heightmap` in a file), an image of the project or of
  the engine (`:`).
  - It is square, 3 to 4096 pixels a side, a pixel per sample: its columns go along +X, its rows along +Z from the top,
    as an image viewer shows them.
  - Its grey level gives the height: black is `GetHeightOffset()` (0 by default), white is the offset plus
    `GetHeightScale()` (10 by default). A colour image counts by its luminance.
  - A PNG of 16 bits keeps 65,536 steps: 0.2 mm for a scale of 12 m. One of 8 bits keeps 256, enough for gentle ground
    but visible as terraces on a slope.
  - The spacing between samples is `SetSpacing` (`spacing` in a file).
  - The map is read when the scene loads, and again when its scale or offset change. The heights themselves are never
    saved: the map makes them again.
  - A map that cannot be read leaves the field empty; `GetHeightmapError()` and the inspector say why.
- **From a script**: `SetHeights(sampleCount, spacing, heights)`, as before. The field then leaves its height map, and
  nothing saves those heights: a procedural world makes them again each run. `GetHeights()` reads them back.
- **Where things stand**: the sample (x, z) is at `(x * spacing, height, z * spacing)` from the actor, so the actor
  stands at the terrain's first corner, and each cell is cut along its (x, z)–(x + 1, z + 1) diagonal.
- **Queries**: `GetHeightAt(x, z)` and `GetNormalAt(x, z)` take a point of the world and answer on the triangles the
  physics uses, or `nil` outside the field.
  - They are exact for an actor turned about Y only; a field tilted about another axis has no single height under a
    point.
  - They cost a few multiplications, with no ray: use them to stand things on the ground.

## The terrain

- **Chunks**: the field is cut into square chunks of `SetChunkSize` cells (32 by default; `chunk_size`). A side that
  does not divide leaves a smaller last row and column. Each chunk is one draw, culled by the frustum like a model.
- **Levels of detail**: up to 4 (`SetLodCount`, `lod_count`).
  - Level n keeps one sample in 2^n along each side, and always the chunk's last row and column, so neighbours at the
    same level meet.
  - A chunk leaves its full detail at `SetLodDistance` from the camera (50 by default; `lod_distance`), the next level
    starts twice as far, and so on. The distance is measured to the chunk's box, not to the actor.
  - A margin of a tenth keeps a chunk from flickering between two levels when the camera stands at a boundary.
  - Every pass of a frame (shadows, reflections, the view) draws a chunk at the same level.
  - Each view picks its own levels from its own camera. In the editor, the scene view keeps levels of its own, while
    what the terrain reports (below) follows the scene's main camera, the one the game view and the game draw with.
  - A chunk's level is built the first time it is drawn, then kept.
- **Skirts**: a strip hangs under each edge of a chunk, as deep as the largest gap its coarsest level leaves along that
  edge. It hides the gaps between neighbours at different levels. The skirts are drawn from both sides, and casting a
  shadow they lengthen the terrain's shadow by their depth at most.
- **Normals** come from the full detail at every level, so the light does not jump when a chunk changes level.
- **The look**:
  - The material is `:Materials/Terrain.ovmat` by default (`Standard`, rough, casting and receiving shadows), or any
    other (`SetMaterialPath`, `material`).
  - The terrain draws with a copy of it, so two terrains may share a material.
  - Its albedo map gets the colour map, stretched over the whole terrain, a pixel centre on each sample for a map of
    one pixel per sample:
    - `SetColorMap("Textures/HillsColor.png")` (`color_map`), an image laid like the height map, its top row along the
      far edge (+Z);
    - or `SetColors(sampleCount, colors)` from a script: red, green and blue from 0 to 1 per sample, in the order of
      the heights. Like heights from a script, these colours are not saved.
- **What it reports**:
  - `GetError()` says why nothing is drawn: no height field on the actor, no heights, or no material;
  - `GetChunkCount()`, `GetChunkLod(chunk)` and `GetTriangleCount()` (the surface at the levels the main camera drew
    last) show the work done. A scene without a main camera reports the view drawn last.

## Cost

- The chunks are built on the CPU when first drawn, at their level: a chunk of 32 cells is 33 × 33 vertices at full
  detail, plus its skirts. Building every level of a 1,025-sample terrain at once would take a moment, so levels are
  built as the camera needs them.
- A change of heights (a new height map, `SetHeights`, a new spacing) or of chunk settings builds the chunks again.
