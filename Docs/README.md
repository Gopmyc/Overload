# Adventurer manual

What a game made with Adventurer must know about the engine: its rules, and the pitfalls learned while building a game
on it. Each page states the behaviour of the current version; [`CHANGELOG.md`](../CHANGELOG.md) says when it changed.

| Page | What it covers |
| --- | --- |
| [Projects](Projects.md) | The project file and its settings |
| [Command line](CommandLine.md) | Running a project with `OvGame`: options, captures, exit codes |
| [Scripting](Scripting.md) | The Lua sandbox, modules, Behaviours and their lifecycle, actors and references, update order, time |
| [Standard library](StandardLibrary.md) | `Class`, `Signal`, `Timers`, `Tween`, `Ease`, `Pool` and `StateMachine`, shipped in Lua with the engine |
| [Navigation](Navigation.md) | Navigation meshes built at run time from static colliders, paths, nearest points, walks along the mesh |
| [Physics](Physics.md) | Bodies, triggers, queries, Jolt specifics, static bodies, height fields, ragdolls |
| [Rendering](Rendering.md) | Materials and per-renderer values, the post process stack and fog, culling, UI text and fonts |
| [Particles](Particles.md) | Particle systems: their settings, bursts from a script, time, billboards and models, the cost |
| [Shaders](Shaders.md) | The shader API and its version, public includes, `Standard`'s extension points, the fog option, custom shaders |
| [Animation](Animation.md) | Bones, layers and masks, clips on other rigs, extra bones, attachments, animation graphs, look at, spring bones, when the pose is worked out |
| [Saving](Saving.md) | The save folder, `Storage` and `Json` |
| [Input](Input.md) | Keys, mouse, the editor's focus rules |
| [Coordinates](Coordinates.md) | Axes, angles and rotation order |
| [Testing](Testing.md) | The engine's tests (`OvTests`) and the in-game checks, how to run them and add one |

The Lua API reference is in [`Resources/Engine/Lua/`](../Resources/Engine/Lua), one definition file per type, to point a
Lua language server at. The UI library documents itself in
[`Resources/Engine/Libraries/UI/Docs/`](../Resources/Engine/Libraries/UI/Docs).
