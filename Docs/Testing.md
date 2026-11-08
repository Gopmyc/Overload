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

`Tests/run_game_checks.sh` runs each scene of `Tests/GameChecks` with a built `OvGame`, hidden, for 600 frames at
most, with `--fail-on-error`:

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
| `Shaders` | Every engine shader loads, all its variants compiled; a shader built on `Standard`'s four extension points draws; the fog option on `Standard` and `Unlit` |

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
