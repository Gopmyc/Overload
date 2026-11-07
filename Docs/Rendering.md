# Rendering

## Per-renderer values

- `MaterialRenderer:SetUserMatrixElement(row, col, value)` feeds a per-renderer 4×4 matrix to the shader
  (`ubo_UserMatrix`), so one material can show a different colour or fill on every instance.
- The matrix is stored row-major. Write off-diagonal values in symmetric pairs so the shader reads the same whatever
  the order, and only write a value that changed.
- `ModelRenderer:SetMaxDrawDistance(metres)` (`max_draw_distance` in a file): beyond it, the model is not drawn.
- `SetVisibilityFlags` chooses the passes a renderer takes part in (geometry, shadows, reflections).

## Culling

- Frustum culling places a model's bounding sphere through its world matrix, so a mirrored model (a negative scale on
  any axis) is culled where it is drawn. The sphere's radius grows with the largest scale.
- A renderer whose vertices are moved far by its shader (particles) also needs custom bounds that hold them all.

## The post process stack

- A scene's render settings live on its post process stack: tonemapping, auto exposure, bloom, FXAA; colour grading
  (`grading_temperature`, `grading_tint`, `grading_saturation`, `grading_contrast`) and the vignette
  (`vignette_intensity`, `vignette_smoothness`), applied in the tonemapping pass; the distance fog (`fog_enabled`,
  `fog_color`, `fog_intensity`, `fog_start`, `fog_density`, `fog_max_opacity`).
- From Lua, they are read and written as a whole with `Get`/`SetFogSettings`, `Get`/`SetTonemappingSettings` and
  `Get`/`SetAutoExposureSettings`. A key missing from a scene keeps the neutral default: no fog, no grading.

## Custom shaders

- The engine's shader includes are under `:Shaders/Common/` and `:Shaders/Lighting/`: `Buffers/EngineUBO.ovfxh` (the
  engine buffer: `ubo_Model`, `ubo_View`, `ubo_Projection`, `ubo_ViewPos`, `ubo_UserMatrix`, `ubo_GameTime`,
  `ubo_FogColor`…), `Fog.ovfxh`, `Lighting/PBR.ovfxh` (`PBRLightingModel`), `Instancing.ovfxh`, `Skinning.ovfxh`,
  `Utils.ovfxh`. A shader that copies `Standard` must be kept in step with it when the engine changes.
- The fog is not a post process pass: every shader applies it. Include `:Shaders/Common/Fog.ovfxh` after the engine
  buffer and call `ApplyFog(color, FragPos)` on a surface's final colour, or `FogFade(alpha, FragPos)` on a blended
  effect's opacity. A shader that forgets it shows as a sharp shape against the haze.
- A lit custom shader that receives shadows declares `#pass SHADOW_PASS` and discards below
  `u_ShadowClippingThreshold` there, and declares `_ShadowMap`, `_LightSpaceMatrix` and `_EnvironmentMap`, which the
  engine binds to a material that receives shadows and reflections.
- Identical meshes are drawn as one instanced draw when their shader declares `#engine_capability INSTANCING` and reads
  its matrices through `:Shaders/Common/Instancing.ovfxh` (`InstanceModelMatrix`, `InstanceNormalMatrix`,
  `InstanceUserMatrix(gl_InstanceID)`): the opaque ones of one material and mesh, and blended ones of one material side
  by side in the drawing order. A value passed from the vertex to the fragment stage per instance is `flat`.
- A material may ask for many instances of its model (`gpu_instances`), which its vertex shader places from
  `gl_InstanceID`: a cloud of particles in one draw.
- A sky drawn after the opaque geometry pushes its depth just short of the far plane (0.99999); any closer to 1 and the
  depth buffer's rounding drops whole triangles.

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
