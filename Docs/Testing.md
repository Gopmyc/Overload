# Testing

The engine is tested in two ways, both run by the CI on every pull request:

- **`OvTests`**, a test program on [doctest](https://github.com/doctest/doctest): what can be checked without a window or
  a GPU.
- **The in-game checks** of `Tests/GameChecks`: a project that `OvGame` runs through its command line (see
  [Command line](CommandLine.md)), for what needs a scene, scripts and a renderer.

## OvTests

It is built with the rest of the engine, into `Bin/<configuration>/OvTests`:

```sh
./gen_proj.sh && make -j$(nproc) config=release_x64 OvTests
Bin/Release/OvTests/OvTests
```

It covers:

| Suite | What it checks |
| --- | --- |
| OvGame command line | Every option, the project file or folder, the refused options and their messages |
| PNG writer | An image reads back pixel for pixel, CRCs and Adler-32 included, over one or several deflate blocks |
| Save storage | The paths a script may name, writing and reading, the errors, the limits, links never followed |
| Lua Json, Lua Storage | The bindings in a bare Lua state: round trips, refused values, the project's limits, the sandbox |
| Project settings | Layer names and ignored pairs, the save folder's name, the runtime limits and their ranges |
| Physics layers, queries, engine | Layers by name, bodies that go through each other, casts and overlaps on a column of boxes, a second engine |
| Typed input | Typed characters kept in UTF-8 from one to four bytes, control characters and values that are no character left out, repeats counted per key, a frame forgetting both |
| Standard library | `Class` inheritance and final names, `Signal` order and changes during an emit, `Pool` takes and evictions, `StateMachine` enter and exit, `Ease` curves and approach, `Timers` and `Tween` advancing on game time by themselves, failures dropped alone, wrong arguments refused |
| Particles | The rate over uneven frames, each particle born at its own time; bursts each cycle, a system that stops after one, a frame longer than a cycle; a full ring giving the oldest slot; seeds; every shape's births and ways; motion against a fine integration, with and without drag; size, colour, fades, rotation and death over a life; bounds holding every particle; births along a moving emitter's way, a local system's space; a script's overrides; the slots sent to the GPU, and the clock moved back without moving a particle; the component on a scene without GPU, played, simulated, saved and read back |
| Blend modes | Each mode's blend function, alpha keeping a pass's own; the mode in a material's state mask beside the other settings; saved and read back, a material saved before it and a value no mode has read as alpha |
| Navigation | A path through a wall's gap, longer than the straight line and kept the agent's radius from the wall; an island's path stopping at the nearest point, incomplete; the nearest point and its extents; walks along the mesh stopped by a wall or passing the gap; a step climbed or not by the agent's climb; tiles rebuilt after a door shuts the gap; random points within a radius and repeated by a seed; settings brought into range, no geometry and too many tiles refused; a scene's static colliders collected by shape and layer, triggers and moving bodies left out, faces turned outwards |
| Component references | Handles that work through a member function, a lambda and a base usertype's method; a removed component and a freed actor's raising a clear error and testing as gone, the UI ones as well; equality whatever the usertype, once gone too; a self that is another userdata, nil or another component refused with a clear error |
| UI clip | An upright element's rectangle, a clip inside a clip and clips that don't meet, a turned element, a world space canvas scaled down, an element entirely outside, the property saved only when set and read back |
| Fonts | Basic Latin and Latin-1 baked with an atlas, Cyrillic, Greek and Latin Extended baked on demand and once, a character the font lacks, an atlas that grows without moving its glyphs, UTF-8 decoding, a font that is no font |
| Json | Values read with their members' order, escapes beyond the first plane, errors with their line and column, nesting, a value written then read back |
| Input actions | Every input's name and the Lua enum Key, the actions file and its mistakes, actions pressed, held and released over frames, a tap within a frame, a stick's way and a trigger, the input pressed in a frame, swaps, resets, bindings saved and read back |
| Input consumption | A button consumed as it goes down or while held, its release, a tap within a frame, a press afresh, pads, a stick's way until it is back to the centre, the captured keyboard, the scroll, and actions read again within the frame |
| Gamepads | Connection and names, buttons pressed, held and released frame by frame, a pad unplugged, the sticks' disc dead zone and the triggers' range |
| Physics interpolation | A body drawn between its last two steps and the same physics without it, teleports in and out of the fixed update, a character's position but not its rotation, kinematic and static bodies left alone |
| Character controller | Landing and standing, slopes it walks and slopes it slides down, walls and the real velocity, steps, drops and ledges, jumps and ceilings, queries, triggers and collision events, layers, teleports, impulses and pushes |
| Scene | GUIDs, destroyed actors and their children freed at garbage collection |
| Lua API | The bindings and the definitions of `Resources/Engine/Lua` hold the same names, inherited members included |
| Shader API | `Docs/Shaders.md` lists every name of each public shader include, and the version is defined for every shader |
| Lua modules | `require` from the project and the engine's resources, its cache shared with `Resources.GetScript`, and the error for each failure; the global frame events (order, removal, a failing callback); a Behaviour from the engine's resources |

doctest's options pick what runs: `--test-suite="Physics*"`, `--test-case="*limits*"`, `--success` to list every
check, `--help` for the rest.

A binding added to the engine needs its definition in `Resources/Engine/Lua`, and the reverse: the "Lua API" suite
fails otherwise, naming what is missing on either side. A definition that describes what a script implements rather
than what the engine binds (`Behaviour`) is listed in the test; a class with no global (`RagdollPart`) describes a
value's shape and is not compared.

A new test goes into the source file of its subject in `Sources/OvTests/src/OvTests/`, or a new file there: the
project takes every `.cpp` of the folder. `OvTests/TestUtils.h` gives a temporary folder that removes itself, and a
capture of the logs.

## In-game checks

`Tests/run_game_checks.sh` runs each scene of `Tests/GameChecks` with a built `OvGame`, hidden, for 20000 frames at
most, with `--fail-on-error`. A check ends the game itself, and the cap only stops one that never does: the physics
steps in real time, so a check that waits on it takes as many frames as the game draws meanwhile.

```sh
Tests/run_game_checks.sh Bin/Release/OvGame/OvGame            # every check
Tests/run_game_checks.sh Bin/Release/OvGame/OvGame Queries    # one check
xvfb-run -a Tests/run_game_checks.sh Bin/Release/OvGame/OvGame  # on a headless Linux machine
```

A check passes when the game ends with exit code 0 and logs `CHECK PASSED <name>`. The saves of the checks go to a
temporary folder.

| Check | What it checks |
| --- | --- |
| `Destroy` | A destroyed actor and its children leave the scene at the end of the frame |
| `PrefabRefs` | In each instance of a prefab, a reference between two of its actors names the instance's own actor; a reference to an actor of the scene is kept |
| `Queries` | `Physics.CastRay`, `CastSphere`, `OverlapSphere`, `OverlapBox`, layer masks and triggers, from Lua |
| `Settings` | The project file reaches the game: layer names and ignored pairs, `json_max_depth`, `storage_max_file_mib`, `max_frame_delta` |
| `Events` | The global `Update` and `LateUpdate` events: their place around the Behaviours' callbacks, their step, an owner that dies, a removal, a callback added while they run |
| `Shaders` | Every engine shader loads, all its variants compiled; a shader built on `Standard`'s four extension points draws; the fog option on `Standard` and `Unlit`; an additive and a premultiplied material drawn in the transparent pass |
| `Gamepad` | The gamepad API from Lua with no pad connected: the enums, every read false or 0, pads out of range, the dead zone and its range |
| `InputActions` | The project's actions from Lua: their order and bindings, rebinding with its swaps and refusals, resets, the bindings saved in the save folder, the labels a menu shows |
| `UIInput` | The UI library with the engine's input: the controller updating itself once a frame and older manual `Update` calls ignored; the keyboard captured while a text entry has the focus, freed when it loses it, when consumption is off and when its controller goes, across two controllers; a text entry editing UTF-8 characters and refusing those `AllowInput` refuses |
| `Text` | UTF-8 text from Lua: Cyrillic and Greek drawn with their own glyphs, a character the font lacks and a broken byte, measured by `GetContentSize`; it also draws them, for a screenshot (`--frames 4 --screenshot`) |
| `StdLibrary` | The standard library in a running game: timers and tweens advancing with no Behaviour to drive them, real time going on while the game is paused, owners that are actors (destroyed, switched off) or a Behaviour's table |
| `Particles` | Particle systems from Lua: every setting and its range, a script's burst and its overrides, the rate over the frames, game time stopped while real time goes on, `Stop` and `Clear`, refused arguments; it also draws, for a screenshot (`--frames 70 --screenshot`), additive sparks, alpha smoke, lit cubes as particles, and a system in its emitter's space beside one in the world's |
| `Navigation` | Navigation from Lua on a floor, walls, a platform and a ramp of static colliders: the mesh built and its settings, paths around a wall and up the ramp, nearest points, walks along the mesh, random points and their seed, a door shut by `Rebuild` leaving a path that stops short, then opened again, refused settings and arguments, a layer mask that leaves no geometry; it also draws the mesh and a path, for a screenshot (`--frames 20 --screenshot`) |
| `ComponentRefs` | Components from Lua as checked handles: from the actor's getters and adders, removed, of an actor destroyed then freed, equal by component, a box as the actor's physical object and as a ray's hit, a value that is no component refused |
| `UIClip` | Clipping from Lua: the property, off by default, and the UI library's scroll panel clipping its content; it also draws, for a screenshot, a panel with children past its edges, a clip inside a clip, a turned panel, a world space canvas and a scroll panel with an item half out of view |
| `InputConsumption` | Consuming from Lua with no device held: the calls and their refusals, the reads that see what is consumed, the keyboard captured from an `Events.AddUpdate` callback before the Behaviours, across frames |
| `Interpolation` | Two boxes falling side by side, one interpolated from the scene file: the same physics in the fixed update, the interpolated one between its last two steps on every frame, a teleport from a script |
| `Character` | A character controller read from the scene file lands and stands, walks through a body of a layer it ignores and through a trigger, climbs a step, is stopped by a wall, jumps and lands, takes an impulse |

A new check is a scene of `Tests/GameChecks/Assets/Scenes`, named after it, whose Behaviour uses
`Scripts/Checks/Check.lua`:

```lua
local Check = require("Scripts/Checks/Check")

local MyCheck = {}
local oCheck = Check.New("MyCheck") -- the scene's name
local nFrame = 0

function MyCheck:OnUpdate(nDelta)
	nFrame = nFrame + 1
	if nFrame == 2 then
		oCheck:Equal(1 + 1, 2, "what this expects")
		oCheck:Finish() -- logs the verdict and quits
	end
end

return MyCheck
```

`Expect`, `Equal` and `Near` count the expectations; `Finish` logs each failure as an error, so that the run ends with
exit code 2, and quits. A check that never reaches `Finish` fails too: it logs no verdict.

The in-game checks need OpenGL 4.5. The CI runs them on Linux, with Mesa's software renderer under Xvfb; a Windows
runner has no OpenGL 4.5, so on Windows the CI runs `OvTests` only.
