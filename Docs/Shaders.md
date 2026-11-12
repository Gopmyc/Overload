# Shaders

A project's shaders build on the engine's: its public includes, the engine buffer, the uniforms the engine binds, and
the `Standard` shader's stages and extension points. Together they make the shader API, versioned like the Lua API (see
[Scripting](Scripting.md)), so a project's shaders keep working when the engine moves on.

## Version

`Common/ShaderApi.ovfxh` defines `SHADER_API_VERSION_MAJOR` and `SHADER_API_VERSION_MINOR`, now 1.1: 1.1 added
`Common/Particles.ovfxh`. The engine
buffer includes it, so every shader can test them:

```glsl
#if SHADER_API_VERSION_MAJOR != 1 || SHADER_API_VERSION_MINOR < 2
#error "made for the engine's shader API 1.2"
#endif
```

- **A name added** to a public include (a function, a uniform, a field of the engine buffer, an extension point)
  raises the minor version.
- **A name deprecated** keeps working, marked `[Deprecated]` in its include and listed under "Deprecated" in the
  `CHANGELOG.md`, for the rest of the major version.
- **A name removed or changing meaning** only happens to a deprecated name, and raises the major version.

What is not public may change at any version: the includes not listed below, and the names a public include lists as
internal.

## Public includes

A shader includes them by their engine path, `:Shaders/<include>`, after `#version 450 core`. The engine's tests check
this table against the files: a name added to a public include must be listed here, as public or internal.

| Include | Declares | Internal |
| --- | --- | --- |
| `Common/ShaderApi.ovfxh` | `SHADER_API_VERSION_MAJOR`, `SHADER_API_VERSION_MINOR` | |
| `Common/Buffers/EngineUBO.ovfxh` | `ubo_Model`, `ubo_View`, `ubo_Projection`, `ubo_ViewProjection`, `ubo_ViewPos`, `ubo_NormalMatrix`, `ubo_UserMatrix`, `ubo_Time`, `ubo_GameTime`, `ubo_DepthOnly`, `ubo_InstanceCount`, `ubo_FogStart`, `ubo_FogDensity`, `ubo_FogMax`, `ubo_FogColor` | |
| `Common/Constants.ovfxh` | `PI` | |
| `Common/Fog.ovfxh` | `FogAmount(fragPos)`, `ApplyFog(color, fragPos)`, `FogFade(alpha, fragPos)` | |
| `Common/Utils.ovfxh` | `TileAndOffsetTexCoords(texCoords, tiling, offset)`, `IsOrthographic(projection)`, `DistanceFadeAlpha(fragPos, viewPos, fadeStart, fadeLength)`, `Dithering(alpha, fragCoord)`, `ApplyParallaxOcclusionMapping(texCoords, heightMap, tangentViewPos, tangentFragPos, heightScale, minLayers, maxLayers)`, `ApplyParallaxMapping(texCoords, heightMap, tangentViewPos, tangentFragPos, heightScale)`, `IsParallaxOutOfBounds(texCoords, tiling, offset, projection)`, `ConstructTBN(model, normal, tangent, bitangent)`, `ComputeNormal(texCoords, normal, normalMap, TBN)`, `UnPack(packedFloat)` | `IsMasked` (deprecated) |
| `Common/Instancing.ovfxh` | `InstanceModelMatrix(instance)`, `InstanceNormalMatrix(instance)`, `InstanceUserMatrix(instance)` | |
| `Common/Skinning.ovfxh` | `ComputeSkinningMatrix(boneIDs, boneWeights)` | |
| `Common/Particles.ovfxh` | `Particle`, `ParticleAt(instance)`, `ParticleModelMatrix(particle)` | |
| `Common/Physics.ovfxh` | `IsPointInAABB(point, center, halfSize)`, `IsPointInSphere(point, center, radius)` | |
| `Lighting/PBR.ovfxh` | `PBRLightingModel(albedo, metallic, roughness, ao, normal, viewPos, fragPos, shadowMap, lightSpaceMatrix, environmentMap, transmission, refractionIndex)` | `DistributionGGX`, `GeometrySchlickGGX`, `FresnelSchlick`, `GeometrySmith`, `CalculateBRDF` |
| `Standard/StandardVertex.ovfxh` | `StandardVertex` (extension point) | |
| `Standard/StandardFragment.ovfxh` | `StandardClip`, `StandardSurface`, `StandardLighting` (extension points), `StandardSurfaceData`, `u_Albedo`, `u_AlbedoMap`, `u_MaskMap`, `u_AmbientOcclusionMap`, `u_EmissiveMap`, `u_EmissiveColor`, `u_EmissiveIntensity`, `u_Metallic`, `u_MetallicMap`, `u_Roughness`, `u_RoughnessMap`, `u_Specular`, `u_SpecularMap`, `u_Glossiness`, `u_GlossinessMap`, `u_NormalMap`, `u_HeightMap`, `u_HeightScale`, `u_ParallaxClipEdges`, `u_MinLayers`, `u_MaxLayers`, `u_ShadowClippingThreshold`, `u_DistanceFadeStart`, `u_DistanceFadeLength`, `u_TextureTiling`, `u_TextureOffset`, `u_AlphaClippingThreshold`, `u_AlphaDithering`, `u_BuiltInToneMapping`, `u_BuiltInGammaCorrection`, `u_RefractionIndex`, `u_Transmission`, `u_Fog`, `_ShadowMap`, `_LightSpaceMatrix`, `_EnvironmentMap` | `DISABLE_IMAGE_BASED_LIGHTING` |

- **The engine buffer** (`binding = 0`): the model, view and projection matrices, the eye's position, the renderer's
  user matrix (see [Rendering](Rendering.md)), real time and game time in seconds, the depth pre-pass flag, the number
  of instances of an instanced draw, and the scene's fog.
- **The engine binds** `_ShadowMap`, `_LightSpaceMatrix` and `_EnvironmentMap` to a material that receives shadows and
  reflections: a lit shader declares them and passes them to `PBRLightingModel`.
- `Lighting/Light.ovfxh`, `Lighting/Shadow.ovfxh`, `Lighting/IBL.ovfxh` and the other buffers of `Common/Buffers/`
  are internal: `PBR.ovfxh`, `Instancing.ovfxh` and `Particles.ovfxh` include them.
- **The particles** of a particle system's draw: `ParticleAt(gl_InstanceID)` returns the instance's particle (`alive`,
  `position` and `velocity` in the world, `size`, `color`, `rotation`, `life`, `seed`), worked out at the system's
  time; `ParticleModelMatrix(particle)` places a model as it. See [Particles](Particles.md).

## Building on Standard

`Standard.ovfx` is two public includes, one per stage. A shader built on it declares its own passes and features, then
includes both stages; before each include, it may define extension points, whose functions it writes after the
include:

| Extension point | Function | Called |
| --- | --- | --- |
| `STANDARD_VERTEX` | `void StandardVertex(inout vec3 worldPosition, inout vec3 worldNormal, vec2 texCoords)` | Once the vertex is in world space: wind, waves, a swelling |
| `STANDARD_CLIP` | `void StandardClip(vec3 fragPos, vec2 texCoords)` | First in the fragment stage, in every pass, shadows included: it may `discard` |
| `STANDARD_SURFACE` | `void StandardSurface(inout StandardSurfaceData surface, vec3 fragPos, vec2 texCoords)` | Once the material's maps are read, before the lighting. After the alpha tests: clip with `STANDARD_CLIP` |
| `STANDARD_LIGHTING` | `vec3 StandardLighting(vec3 color, StandardSurfaceData surface, vec3 fragPos)` | On the lit colour, emission included, before the fog: a rim light, a tint |

`StandardSurfaceData` holds `albedo` (rgb lit, a written out), `normal` (world space), `metallic`, `roughness`, `ao`
and `emissive`. A shader that only uses some of them leaves the others to `Standard`, which keeps its own behaviour,
uniforms and variants.

```glsl
#pass SHADOW_PASS
#pass REFLECTION_PASS
#feature NORMAL_MAPPING
#engine_feature SKINNING

#shader vertex
#version 450 core
#include ":Shaders/Standard/StandardVertex.ovfxh"

#shader fragment
#version 450 core
#define STANDARD_LIGHTING
#include ":Shaders/Standard/StandardFragment.ovfxh"

uniform vec3 u_RimColor = vec3(1.0);
uniform float u_RimIntensity = 0.0;

vec3 StandardLighting(vec3 color, StandardSurfaceData surface, vec3 fragPos)
{
    const float edge = 1.0 - max(dot(surface.normal, normalize(ubo_ViewPos - fragPos)), 0.0);
    return color + u_RimColor * u_RimIntensity * edge * edge * edge;
}
```

The stages' varyings are `Standard`'s: a shader that needs its own (a colour per vertex, a value per instance) writes
its own stages with the public includes. `Tests/GameChecks/Assets/Shaders/Hooked.ovfx` uses all four extension points.

## The fog

The fog is not a post process pass: each shader applies it, from the engine buffer's `ubo_Fog*` that the scene's post
process stack sets (see [Rendering](Rendering.md)).

- `Standard` and `Unlit` have a material option, `u_Fog`: on by default for `Standard`, off for `Unlit`, which matches
  what they did before the option. Turn it off for what must stay sharp in the haze (a sky, a marker), on for an unlit
  surface that belongs to the scene.
- A custom shader includes `Common/Fog.ovfxh` after the engine buffer, and calls `ApplyFog(color, fragPos)` on a
  surface's final colour, or `FogFade(alpha, fragPos)` on a blended effect's opacity, which fades out instead of
  turning into a patch of fog. A shader that forgets it shows as a sharp shape against the haze.

## Custom shaders

- A lit custom shader that receives shadows declares `#pass SHADOW_PASS` and discards below
  `u_ShadowClippingThreshold` there, and declares `_ShadowMap`, `_LightSpaceMatrix` and `_EnvironmentMap`.
- Identical meshes are drawn as one instanced draw when their shader declares `#engine_capability INSTANCING` and reads
  its matrices through `Common/Instancing.ovfxh` (`InstanceModelMatrix`, `InstanceNormalMatrix`,
  `InstanceUserMatrix(gl_InstanceID)`): the opaque ones of one material and mesh, and blended ones of one material side
  by side in the drawing order. A value passed from the vertex to the fragment stage per instance is `flat`. A shader
  that reads `ubo_UserMatrix` instead does not declare the capability.
- A material may ask for many instances of its model (`gpu_instances`), which its vertex shader places from
  `gl_InstanceID`: a cloud of points in one draw. Particles that are born, live and die are a particle system's
  (see [Particles](Particles.md)).
- A sky drawn after the opaque geometry pushes its depth just short of the far plane (0.99999); any closer to 1 and the
  depth buffer's rounding drops whole triangles.
- A shader compiles every variant of its features when it loads, and logs an error for each one that fails: the
  in-game checks (see [Testing](Testing.md)) load every engine shader.
