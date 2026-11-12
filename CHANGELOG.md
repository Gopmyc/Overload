# Changelog

What Adventurer adds to [Overload](https://github.com/Overload-Technologies/Overload), version by version, newest
first. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the versions follow
[Semantic Versioning](https://semver.org/): a script, a shader or a project made for one version keeps working with any
later version of the same major version.

## [Unreleased]

### Fixed

- **`ReflectionProbe:RequestCapture()` without its argument**: the argument `forceImmediate` is optional, as its
  definition and the C++ say. The binding required it, so a call without it raised an error in a Debug build and read
  an undefined value in a Release build.

## [0.1.0] - 2026-11-07

Adventurer's first version: Overload 1.9 and the changes below.

### Added

- **Engine version**: `VERSION.txt` holds Adventurer's version, `Application.GetEngineVersion()` returns it to scripts,
  and the editor's Help menu shows it beside the Overload version it is based on (`OVERLOAD_VERSION`).
- **Physics on Jolt**: `OvPhysics` is ported from Bullet3 to Jolt Physics 5.6.0.
- **Ragdolls**: `SkinnedMeshRenderer:StartRagdoll(parts, velocity)` hands a skeleton over to capsules joined by
  swing-twist constraints, from its current pose; `StopRagdoll`, `IsRagdoll`, `AddRagdollImpulse`.
- **Static bodies**: `PhysicalObject:SetStatic` and `IsStatic` (`static` in a file), a body that never moves by itself
  and only follows its transform when the transform changes.
- **Height fields**: the `PhysicalHeightField` component, a static Jolt height field set from Lua with
  `SetHeights(sampleCount, spacing, heights)`.
- **Animation layers**: several clips at once on a skinned mesh, each with its own source, speed, loop and weight; the
  clip sources resolved by path inside the component (`SetAnimationSourceModel`).
- **Layer masks**: `SetLayerMask(rootName, layer)` and `GetLayerMask(layer)`, a layer that only drives a node and its
  descendants (`<mask_root>` in a file, shown in the inspector).
- **Clips on rigs of other bone lengths**: a clip plays on a model whose bones share their names, parents, rest
  rotations and rest scales but not their rest positions; each move is scaled by the ratio of the rest lengths.
- **Skeletons that extend the clip's**: a model may carry bones of its own under a bone both rigs share; no track
  drives them and scripts may turn them.
- **Global bone transforms**: `GetBoneGlobalPosition`, `GetBoneGlobalRotation` and `GetBoneGlobalScale` in Lua.
- **Bone attachments**: the `BoneAttachment` component (`SetBone`, `SetOffsetPosition/Rotation/Scale`, `SetTarget`,
  `IsAttached`), an actor that rides a bone, placed after every script.
- **Rigid parts bound to bones**: at import, a mesh hung on a bone without skin weights follows its nearest bone
  ancestor; imported normals and tangents are transformed as directions.
- **2D UI**: canvases, `Image`, `Text` and layout components (Overload PR #819), screen and world space canvases, and the
  UI Lua library in `Resources/Engine/Libraries/UI` with its own documentation.
- **Save files**: `Storage` (`Write`, `Read`, `Exists`, `Delete`, `List`) reaches the project's save folder and nothing
  else, and `Json` (`Encode`, `Decode`) turns a table into text and back. See [Docs/Saving.md](Docs/Saving.md).
- **Models built from Lua**: `Resources.CreateModel(path, geometry, materialName)`, a model of one mesh from flat
  arrays, registered under a path that needs no file and rebuilt in place.
- **Script loading**: `Resources.GetScript` (Overload PR #843), sandboxed module loading for Lua.
- **Time**: the `Time` table, `GetScale`, `SetScale` (never below 0), `GetUnscaledDeltaTime` and `GetGameTime`; the scaled
  game time is `ubo_GameTime` in the engine buffer.
- **Lua additions**: `Application.Quit()`; `Inputs.GetKeyName(key)` names a printable key by the keyboard's layout;
  `Inputs.GetViewportSize` and `Inputs.SetCursorShape`; the renderer's visibility flags; the `string`, `table`,
  `coroutine` and `utf8` libraries in the sandbox.
- **Sleeping Behaviours**: `Actor:SetBehaviourUpdating(behaviour, bool)` and `IsBehaviourUpdating`, a Behaviour that
  stops, then resumes, receiving its per-frame callbacks.
- **Rendering**: a draw distance per model renderer (`SetMaxDrawDistance`, `max_draw_distance` in a file); identical
  opaque meshes, and neighbouring blended meshes of one material, drawn as one instanced draw when their shader
  declares `#engine_capability INSTANCING`; colour grading and a vignette in the tonemapping pass; a distance fog on
  the post process stack; cached directional shadow maps; an optional depth pre-pass.
- **Profiling**: a Tracy zone per Behaviour callback, named `script:function`.

### Changed

- **Behaviours run in their declared order**, the order of the prefab or the scene file, for every callback.
- **The Lua state is kept across scene changes**: removing a Behaviour no longer rebuilds it, so modules and what they
  hold survive `Scenes.Load`. The editor still rebuilds it on play and on stop.
- **A Behaviour's script is compiled once** per Lua state and run for each Behaviour, which still gets a table of its
  own.
- **Actors are found by GUID in constant time**: a scene keeps its actors in a map by GUID.
- **Lua holds actors as checked handles**: a destroyed actor raises an error when used, `IsAlive()` tests it;
  `FindChild` and `GetParent` return `nil` when there is none; `GetChildren` and `FindActorsBy*` return plain tables.
- **Game input in the editor**: the game reads keys, clicks, scroll and the mouse lock only while the Game View has
  the focus; F5 starts and stops play, and Escape belongs to the game.
- **A minimized window** keeps the game running; only drawing stops.

### Removed

- Bullet3, replaced by Jolt Physics.
