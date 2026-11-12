# UI library: engine limitations

Features of the UI library that are missing or incomplete because the engine lacks the primitive,
not just its Lua binding. Features that only need a binding are listed in
[LuaExposureGaps.md](LuaExposureGaps.md).

## 1. No immediate-mode drawing

- **Feature:** `Paint(width, height)` drawing arbitrary content, as with `surface.Draw*` in Garry's Mod.
- **Missing primitive:** a UI drawing API (rectangles, outlines, lines, text at a position). UI rendering
  is retained: only `Image` and `Text` components are drawn.
- **Impact:** `Paint` is called when the visual state changes (size, hover, pressed, enabled, focus) and
  configures the components of the panel. Custom shapes such as borders, rounded corners or gradients
  can only be obtained through textures.

## 2. A zero size axis means "intrinsic size"

- **Feature:** panels with a zero width or height.
- **Missing primitive:** `CTransform::SetUISize` reserves 0 for the intrinsic size of the element
  (texture size, text size).
- **Impact:** the library sends 0.001 instead of 0, so an empty panel doesn't grow to the size of its
  image or text.

## 3. No sibling index API

- **Feature:** `SetZPos`.
- **Missing primitive:** moving an actor to a given index among its siblings (#384). Actors are drawn in
  the order of their parent children list, and `Actor::SetParent` always appends.
- **Impact:** changing the Z position re-attaches every sibling in order, which fires Attach and Detach
  events and costs O(n) per change.

## 4. World space canvases are drawn after the scene

- **Feature:** world space canvases, and panels in the 3D world (`DWorldPanel`, `DWorldPath`,
  `DWorldArc`, see [WorldSpace.md](WorldSpace.md)).
- **Missing primitive:** UI drawn among the lit geometry. Every canvas is drawn by the UI pass, after
  post-processing. A world space canvas is placed in the scene and depth-tested against it, but it never
  writes depth and isn't lit.
- **Impact:**
  - a world space canvas keeps its exact colours: it isn't lit, shadowed, fogged or tonemapped. 3D
    content carried with `DModelPanel` is, like the rest of the scene;
  - two world space canvases overlapping on screen are drawn in the order of the scene, not from the
    farthest to the nearest;
  - world panels on a screen space canvas are still projected on it every frame: a flat panel can't
    be tilted in perspective, and the scene doesn't hide them. `SetOcclusionCheck` hides a
    `DWorldPanel` when a raycast hits another collider, and `SetFrontOnly` hides the far half of a path
    around a character. Only a `DWorldLayer` sorts them between themselves.
