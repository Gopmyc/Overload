# Rendering

## Per-renderer values

- `MaterialRenderer:SetUserMatrixElement(row, col, value)` feeds a per-renderer 4×4 matrix to the shader
  (`ubo_UserMatrix`), so one material can show a different colour or fill on every instance.
- The matrix is stored row-major. Write off-diagonal values in symmetric pairs so the shader reads the same whatever
  the order, and only write a value that changed.
- `ModelRenderer:SetMaxDrawDistance(metres)` (`max_draw_distance` in a file): beyond it, the model is not drawn.
- `SetVisibilityFlags` chooses the passes a renderer takes part in (geometry, shadows, reflections).

## Culling

- Frustum culling ignores the sign of the scale: a mirrored model (negative X scale) is culled with its bounding sphere
  on the wrong side. Give it custom bounds (`frustum_behaviour` 3) centred on its pivot.
- A renderer whose vertices are moved far by its shader also needs custom bounds that hold them all. A particle system
  keeps its own (see [Particles](Particles.md)).

## Blend modes

- A blendable material is drawn in the transparent pass, after the opaque ones, and combined with what is already
  drawn by its blend mode (`blend_mode` in a `.ovmat`, "Blend Mode" in the material editor):

  | Mode | `blend_mode` | Result | For |
  | --- | --- | --- | --- |
  | Alpha | 0 (default) | source × alpha + destination × (1 − alpha) | glass, smoke, fades |
  | Additive | 1 | source × alpha + destination | light that adds up: sparks, glows, fire |
  | Premultiplied | 2 | source + destination × (1 − alpha) | a colour already multiplied by its alpha |

- The blend mode does nothing on a material that is not blendable.
- Alpha keeps the blend function of the pass, which is alpha blending in every scene pass. A post process effect that
  sets its own, as bloom does, keeps it for its alpha materials.
- A material saved before blend modes has none, and blends as it always did.
- A blendable material usually writes no depth (`depth_writing` false). Blended objects are sorted by their centres,
  so two that cross each other would otherwise hide parts of one another. Additive ones add up in any order.

## Particles

Particle systems, drawn by the GPU in one draw each, have a page of their own: [Particles](Particles.md).

## The post process stack

- A scene's render settings live on its post process stack: tonemapping, auto exposure, bloom, FXAA; colour grading
  (`grading_temperature`, `grading_tint`, `grading_saturation`, `grading_contrast`) and the vignette
  (`vignette_intensity`, `vignette_smoothness`), applied in the tonemapping pass; the distance fog (`fog_enabled`,
  `fog_color`, `fog_intensity`, `fog_start`, `fog_density`, `fog_max_opacity`).
- From Lua, they are read and written as a whole with `Get`/`SetFogSettings`, `Get`/`SetTonemappingSettings` and
  `Get`/`SetAutoExposureSettings`. A key missing from a scene keeps the neutral default: no fog, no grading.

## Custom shaders

The shader API, its version, the public includes, `Standard`'s extension points, the fog's material option and the
rules of a custom shader are in [Shaders](Shaders.md).

## Models built from Lua

- `Resources.CreateModel(path, geometry, materialName)` builds a model of one mesh from flat arrays (`Positions`,
  `Indices` from 0 and counter-clockwise, and optional `Normals`, `TexCoords`, `Tangents`, `Bitangents`), registered
  under a path that needs no file. Built again at the same path, it is replaced in place under every renderer using it.

## UI text

- A `Text` component draws a UTF-8 string with a TrueType font, Roboto (`:Fonts/Roboto-Regular.ttf`) by default.
- **Glyphs**:
  - a font keeps one glyph atlas for each whole pixel size it is drawn at, from 1 to 256: a game that uses few sizes
    keeps few atlases;
  - Basic Latin and Latin-1 are baked with an atlas; any other character the font has (Latin Extended, Greek,
    Cyrillic in Roboto) is baked the first time a text needs it, and the atlas grows, up to 4096 pixels a side;
  - a character the font lacks is drawn as `?`, and a byte that starts no valid UTF-8 sequence as U+FFFD (or `?`
    when the font lacks that too). Roboto has no CJK and no emoji: a project that needs them sets a font that has
    them.
- `Text:GetContentSize()` gives the size the lines take, in canvas pixels, wrapped at the element's width when it has
  one: what a label sizes itself to.
- **A custom text shader** samples `u_FontAtlas` at the texture coordinates divided by
  `textureSize(u_FontAtlas, 0)`. They are in atlas pixels, which stay put when the atlas grows.

## UI clipping

- An element with `Transform:SetUIClipChildren(true)` ("Clip Children" in the inspector) hides what its descendants
  draw outside its rectangle. It is not clipped itself, and a canvas below it starts afresh.
- **Clips inside clips**: upright elements of a canvas show what all their clips let through. A turned element, or one
  inside a turned clip, clips to its own rectangle: the innermost cuts.
- The test is made by the `Image` and `Text` shaders, pixel by pixel, rather than by a scissor rectangle: it holds on a
  world space canvas, on a turned element, and in the editor's views. A drawable entirely outside an upright clip is not
  drawn at all.
- **A custom image or text shader** clips when it declares `uniform mat4 u_ClipMatrix` and `uniform bool
  u_ClipEnabled`, takes `(u_ClipMatrix * ubo_Model * vec4(geo_Pos, 1.0)).xy` to the fragment stage, and discards there
  when it is enabled and either coordinate is beyond -1 or 1, as `Image.ovfx` does. Without them, its drawables are
  only left out when entirely outside.
- **Limit**: the editor's picking still sees the clipped part of an element.

## Textures

- Textures wrap in `REPEAT` by default. A UI texture needs a `.png.meta` that switches it to clamp, or seams appear:

  ```ini
  HORIZONTAL_WRAP=1
  VERTICAL_WRAP=1
  ```
