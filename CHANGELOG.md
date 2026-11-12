# Changelog

What Adventurer adds to [Overload](https://github.com/Overload-Technologies/Overload), version by version, newest
first. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the versions follow
[Semantic Versioning](https://semver.org/): a script, a shader or a project made for one version keeps working with any
later version of the same major version.

## [Unreleased]

### Added

- **Collision layers**: 16 layers, one per physical object (`SetCollisionLayer`, `GetCollisionLayer`,
  `collision_layer` in a file, a list in the inspector). The project names them (`physics_layer_names`) and lists the
  pairs that go through each other (`physics_ignored_layer_pairs`); Lua reads and changes them at run time
  (`Physics.GetLayerIndex`, `GetLayerName`, `DoLayersCollide`, `SetLayersCollide`). See [Docs/Physics.md](Docs/Physics.md).
- **Physics queries**: `Physics.CastRay` and `Physics.CastSphere` return the objects met, nearest first, with the
  point, the normal and the distance; `Physics.OverlapSphere` and `Physics.OverlapBox` return the objects a shape
  overlaps. Each takes a layer mask (`Physics.GetLayerMask`) and leaves triggers out unless asked. `Physics.Raycast`
  is unchanged.
- **Project ID**: the save folder follows the project's `project_id`, or `executable_name` while it is empty, so that
  a game renamed keeps its saves. The editor shows it beside the executable name.
- **Runtime limits in the project**: the save folder's limits (`storage_max_file_mib`, `storage_max_total_mib`,
  `storage_max_entries`), `Json`'s (`json_max_mib`, `json_max_depth`) and the longest frame step (`max_frame_delta`)
  are optional project keys; their defaults are the former fixed values. In C++, `SaveStorageLimits`,
  `Clock::SetMaxDeltaTime` and `OvCore::Scripting::SetJsonLimits`.
- **Play shortcuts**: the editor's keys that start and stop playing are set in Settings > Play Shortcuts (F5 for
  both by default, as before).
- **`OvGame` command line**: `--project` runs a project from its folder, with `--engine`, `--scene`, `--frames`,
  `--screenshot` (a PNG of the last frame), `--hidden-window` and `--fail-on-error`; the exit code tells how the run
  went. Without options, a built game runs as before. See [Docs/CommandLine.md](Docs/CommandLine.md).
- **Engine tests**: `OvTests`, a doctest program built with the engine, tests the command line, the PNG writer, the save
  storage, the Lua `Json` and `Storage` bindings, the project settings, physics layers and queries, and scenes; the
  in-game checks of `Tests/GameChecks` run the engine through `OvGame` (destroyed actors, prefab references, Lua
  queries, project settings). The CI runs both, the in-game checks on Linux. See [Docs/Testing.md](Docs/Testing.md).
- **Lua module contract**: `require(path)` loads a module of the project's assets, or of the engine's with `:`,
  shares `Resources.GetScript`'s cache, and raises an error that says why a module cannot be loaded.
  `Application.GetApiVersion()` returns the version of the Lua API, with a deprecation policy in
  [Docs/Scripting.md](Docs/Scripting.md). `OvGame --dump-lua-api` prints the API, and the engine's tests check it
  against the definitions of `Resources/Engine/Lua`, which now cover every binding (`AssetRef`, the settings'
  constructors, the matrices' call, Tracy's stack-capturing zones).
- **Shader API**: the public includes, their functions and uniforms, the engine buffer and `Standard`'s stages are
  declared in [Docs/Shaders.md](Docs/Shaders.md) and versioned (`SHADER_API_VERSION_MAJOR`/`MINOR`, 1.0, in every
  shader through the engine buffer); the engine's tests check the declaration against the includes.
- **Standard's extension points**: `Standard` is two public includes (`Standard/StandardVertex.ovfxh`,
  `Standard/StandardFragment.ovfxh`) that a shader includes and extends through `STANDARD_VERTEX`, `STANDARD_CLIP`,
  `STANDARD_SURFACE` and `STANDARD_LIGHTING`, instead of copying `Standard` and falling behind it. `Standard` draws as
  before, pixel for pixel.
- **Global frame events**: `Events.AddUpdate(callback[, owner])`, `Events.AddLateUpdate` and `Events.Remove` run a
  module every frame, before every Behaviour's `OnUpdate` and after every `OnLateUpdate`, without an actor in the scene.
  A callback that fails is logged and removed, one with an owner ends with it. The Lua API is now 1.1. See
  [Docs/Scripting.md](Docs/Scripting.md).
- **Behaviours from the engine's resources**: a Behaviour whose path starts with `:` (`:Behaviours/Spin.lua`) is loaded
  from the engine's resources, as `require` loads a module.
- **Fog as a material option**: `u_Fog` on `Standard` (on by default) and `Unlit` (off by default, now able to take the
  fog), as both behaved before.
- **Character controller**: `CharacterController` (`Actor:AddCharacterController`, the "Character Controller" component)
  moves a character by its velocity on Jolt's `CharacterVirtual`: it stands on the slopes it can walk without sliding,
  climbs steps, keeps to the ground going down and falls under gravity. `GetRealVelocity()` says how fast it really
  went, `IsGrounded()`, `GetGroundState()` and `GetGroundObject()` what holds it. Queries, triggers and collision
  events see it as a body; `physics_ragdoll_ignores_dynamic_bodies` lets ragdolls through it. The Lua API is now 1.2.
  See [Docs/Physics.md](Docs/Physics.md).
- **Physics interpolation**: `PhysicalObject:SetInterpolated(true)` (`interpolated` in a file, a box in the inspector)
  draws a dynamic body or a character controller between its last two physics steps, so that it and what follows it
  move smoothly above 60 FPS. `OnFixedUpdate` still sees the pose of the physics, and a script that moves the transform
  teleports the body. Off by default. The Lua API is now 1.3.
- **Gamepads**: up to 4, through GLFW's gamepad mappings. `Inputs.GetGamepadButton`, `GetGamepadButtonDown`,
  `GetGamepadButtonUp` and `GetGamepadAxis` (sticks with a disc dead zone, up positive; triggers 0 to 1), on a given pad
  or the first connected; `IsGamepadConnected`, `GetGamepadName`, `SetGamepadDeadZone`. They follow the Game View's
  focus in the editor as the keyboard does. The Lua API is now 1.4. See [Docs/Input.md](Docs/Input.md).
- **Input actions**: a project declares its actions and their default inputs (keys, mouse buttons, gamepad buttons,
  triggers, a stick's way) in a JSON file its `input_actions` key names. `Inputs.GetAction`, `GetActionDown`,
  `GetActionUp` and `GetActionValue` read them; `GetBindings`, `SetBinding` (which swaps with the slot that held the
  input), `ResetBindings`, `GetPressedInput` and `GetInputName` make a rebinding menu, and the bindings are saved in the
  save folder. The Lua API is now 1.5. See [Docs/Input.md](Docs/Input.md).
- **JSON in C++**: `OvTools::Utils::Json` reads and writes JSON values, for the engine's own files.
- **Input consumption**: `Inputs.Consume(input[, pad])` hides an input from the game's reads, direct or through the
  actions, until it is released, and releases it for the game if it saw it held; `Inputs.ConsumeMouseScroll()` hides
  the frame's scroll, and `Inputs.SetKeyboardCaptured(captured)` every key, for a text field. Every read takes a last
  `includeConsumed` argument, for the code that consumed. The Lua API is now 1.6. See [Docs/Input.md](Docs/Input.md).
- **Text beyond Latin-1**: a font bakes any character it has the first time a text needs it (Greek and Cyrillic in
  Roboto), its atlas growing up to 4096 pixels a side; a character it lacks is still drawn as `?`.
  `Text:GetContentSize()` measures a text. The Lua API is now 1.7. See [Docs/Rendering.md](Docs/Rendering.md).
- **Typed text**: `Inputs.GetTypedText()` gives the characters typed during the frame, in UTF-8, as the keyboard's
  layout makes them (accents, dead keys, AltGr), and `Inputs.GetKeyRepeatCount(key)` the system's repeats of a key held
  down. The Lua API is now 1.8. See [Docs/Input.md](Docs/Input.md).
- **The UI library takes its input**: `DTextEntry` takes the characters typed, any the font has, instead of a US
  layout guessed from the keys, and repeats at the system's rate. The controller keeps from gameplay a click that
  lands on a panel, the wheel over a panel, and the keyboard while a panel has the focus (`SetConsumesInput(false)`
  lets them through). It updates itself, after every `OnLateUpdate`.

### Changed

- **The UI controller updates itself**: a `controller:Update(deltaTime)` from a Behaviour now changes nothing after
  the frame the controller is created in, and can be removed. Panel callbacks run after every `OnLateUpdate`; a
  gameplay script that read clicks meant for the UI no longer sees them.
- **Text texture coordinates are in atlas pixels**: a custom text shader divides them by
  `textureSize(u_FontAtlas, 0)`, as `Text.ovfx` does, so that a growing atlas moves no glyph.
- **No audio device is a warning**: the audio engine logs a warning, no longer an error, when it cannot open a
  device. The game runs on without sound, as before.
- **Ragdolls collide by their layer**: their bodies are on `physics_ragdoll_layer` (15 by default). Going through
  dynamic bodies is now the project option `physics_ragdoll_ignores_dynamic_bodies`, off by default. A project that
  relied on it sets it to `true`.

### Removed

- **Constructors no script could use**: `Scene.new`, `PhysicsHit.new`, `RaycastHit.new` and `AssetRef.new`, made by
  sol on its own, are gone. A scene, a hit or a ray's result come from the engine; an asset reference from `Model()`,
  `Prefab()` and the like.

### Fixed

- **The save folder's entry limit counts new folders**: a write that makes folders on the way to its file counts them
  against `storage_max_entries`. Before, it counted the file only, so the folder could hold up to three entries too
  many.
- **A destroyed physics engine stops listening for new bodies**: a body made after an engine was destroyed reached it
  in freed memory. The game and the editor keep one engine, so only a program that makes several, such as the tests,
  could meet it.
- **Destroyed actors are freed in a built game**: `OvGame` collects the actors destroyed during a frame at its end, as
  the editor's play mode always did. Before, a destroyed actor stayed active and drawn in a built game.
- **Actor references inside a prefab survive instantiation**: an actor property that names another actor of the same
  prefab now names that actor's instance. Before, every instance got new GUIDs and such a property came out `nil`.
- **The editor completes a project file instead of resetting it**: a project file lacking a setting gets that setting's
  default and keeps all its other values. Before, one missing key reset every setting (`start_scene` emptied,
  `executable_name` back to `Game`, which also moved the save folder).

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
