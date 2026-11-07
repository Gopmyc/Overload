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
- A renderer whose vertices are moved far by its shader (particles) also needs custom bounds that hold them all.

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

## Textures

- Textures wrap in `REPEAT` by default. A UI texture needs a `.png.meta` that switches it to clamp, or seams appear:

  ```ini
  HORIZONTAL_WRAP=1
  VERTICAL_WRAP=1
  ```
