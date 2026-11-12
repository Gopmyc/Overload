# Navigation

The engine finds paths for characters on a navigation mesh, built at run time from a scene's static colliders with
[Recast & Detour](https://github.com/recastnavigation/recastnavigation). The mesh is the floor an agent of a given size
can stand on: kept its radius from walls and edges, under ceilings it fits beneath, joined across steps it can climb.
The engine gives paths; moving along them, on a character controller or otherwise, is the game's.

```lua
Navigation.Build({ agentRadius = 0.4, agentHeight = 1.8 })

local corners, complete = Navigation.FindPath(guard:GetTransform():GetWorldPosition(), target)
if corners then
	-- walk from corner to corner; when not complete, the last corner is the nearest reachable point
end
```

## Building

- `Navigation.Build([settings])` builds the current scene's mesh, replacing the one built before, and returns `true`,
  or `false` and why not. Build it once the scene's colliders are in place: after a procedural world is generated,
  not in the `OnStart` of the actor that generates it.
- **What goes in**: the static colliders of the scene's active actors, in the collision layers of `layerMask`.
  - Height fields, boxes, spheres and capsules.
  - Each shape is the one the physics makes, the actor's scale included. Spheres and capsules are a mesh slightly
    larger than the shape.
  - Triggers are left out, and so are dynamic and kinematic bodies, which move.
  - Models without a collider are not seen: the mesh follows what characters collide with, not what is drawn.
- **The settings**, each brought into its range:

  | Setting | Default | What it does |
  | --- | --- | --- |
  | `agentRadius` | 0.4 | How far a path keeps from walls and edges |
  | `agentHeight` | 1.8 | Leaves out what is under a ceiling lower than this |
  | `agentMaxClimb` | 0.4 | The highest step walked up or down |
  | `agentMaxSlope` | 45 | The steepest ground walked on, in degrees |
  | `cellSize` | 0.2 | The grid the colliders are sampled on, across: finer is more exact and slower |
  | `cellHeight` | 0.1 | The same, upwards |
  | `tileSize` | 48 | Cells across a tile, the part `Rebuild` makes again |
  | `layerMask` | 65535 | The collision layers whose colliders are taken (see `Physics.GetLayerMask`) |

- A setting the table does not know, or a tile size or layer mask that is not a whole number, raises an error.
- `Navigation.GetSettings()` returns the settings of the mesh built.
- `Navigation.IsBuilt()`, `Navigation.GetPolygonCount()` and `Navigation.Clear()`.
- **Changes**: `Navigation.Rebuild(min, max)` builds again the tiles a box touches, from the colliders there now. Use it
  after a door shuts, a bridge falls or a wall goes up. A collider destroyed this frame is freed at its end: rebuild in
  the next.
- **One mesh per scene**, freed with it. A scene loaded starts with none.
- **Limits**:
  - A mesh spans at most 16384 tiles: raise `tileSize` or `cellSize` beyond that.
  - It is built on the main thread, in the call: build it while a loading screen shows.

## Queries

- `Navigation.FindPath(from, to)` returns the path's corners, the start and the end included, and whether it reaches
  the end. Both points are first taken to the nearest point of the mesh, within 2 m across and 4 m up or down.
  - A path that cannot reach the end (an island, a shut door) ends at the reachable point nearest to it, with
    `complete` false.
  - Too far from the mesh, or with no mesh built, it returns `nil` and why.
- `Navigation.FindNearestPoint(point[, extents])` returns the nearest point of the mesh within the box of half sizes
  `extents` (2, 4, 2 by default) around the point, or `nil`.
- `Navigation.Raycast(from, to)` walks along the mesh, as seen from above, from a point towards another. It returns
  whether an edge stopped it, where it stopped (on the mesh), and the edge's normal. This answers whether a straight
  walk is free, without a path.
- `Navigation.GetRandomPoint()` returns a point of the mesh, the larger polygons the likelier. Given a center and a
  radius, `Navigation.GetRandomPoint(center, radius)` returns one within that distance and reachable from it.
  `Navigation.SetSeed(seed)` makes them repeatable.
- `Navigation.GetDebugGeometry()` returns the mesh's surface in the form `Resources.CreateModel` takes, to draw it:

  ```lua
  local model = Resources.CreateModel("Generated/NavigationMesh", Navigation.GetDebugGeometry(), "Overlay")
  ```

## Cost

Measured on Linux, Release, on the Adventurers World: a terrain height field and its props, about 150000 triangles.

| Step | Time |
| --- | --- |
| Collecting the colliders | 17 ms |
| Building 729 tiles (27 by 27) | 480 ms |
| A path between two random points, about 6 corners | 0.18 ms |
