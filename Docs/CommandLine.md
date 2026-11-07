# Command line

`OvGame`, the program a built game runs, also runs a project straight from its folder: for a test, a capture or a
benchmark, without building the game first.

## Without options

A built game runs as before: the project in `Data/User` (its settings in `Data/User/Game.ini`, its assets in
`Data/User/Assets`), the engine's resources in `Data/Engine`, the window shown, until the game quits.

## Options

| Option | What it does |
| --- | --- |
| `--project <folder or file>` | Runs this project: a `.ovproject` file, or the folder that holds exactly one. Its assets are the `Assets` folder next to it |
| `--engine <folder>` | The engine's resources, an Adventurer checkout's `Resources/Engine` (`Data/Engine` by default) |
| `--scene <path>` | Starts on this scene, relative to the project's assets, instead of the project's `start_scene` |
| `--frames <n>` | Quits after n frames |
| `--screenshot <file.png>` | Writes the last frame to a PNG image. Needs `--frames` |
| `--hidden-window` | Does not show the window. The game still draws each frame |
| `--fail-on-error` | Ends with exit code 2 when an error was logged |
| `--dump-lua-api` | Prints the engine's Lua API, one name per line (`Physics.CastRay`, `Vector3.x`), and quits without a window. See [Scripting](Scripting.md) |
| `--help`, `-h` | Prints the options |

The paths are relative to the current folder. For example, ten seconds of a scene at 60 frames per second, its last
frame captured:

```sh
OvGame --project ../MyGame --engine ../Adventurer/Resources/Engine --scene Scenes/Level1.ovscene \
       --frames 600 --screenshot level1.png --hidden-window --fail-on-error
```

## Exit codes

| Code | Meaning |
| --- | --- |
| 0 | The run went well |
| 1 | An option is wrong, or the start scene cannot be loaded (the game then runs an empty scene until it quits) |
| 2 | With `--fail-on-error`: an error was logged, from the start scene's loading to the game's shutdown |
| 3 | The screenshot was not written: it cannot be, or the game quit before the last frame |

1 and 3 take precedence over 2: a run whose screenshot fails ends with 3, even though that failure is also logged as
an error.

## What to know

- **A frame is the game's own frame.** `--frames` counts the frames the game drew, whatever the time they took: on a
  slow machine, 600 frames last longer than ten seconds, and each frame's step stays capped by `max_frame_delta` (see
  [Projects](Projects.md)).
- **The screenshot is the scene as drawn**, post process and game UI included, at the window's size. The debug
  overlay (F12) is not in it.
- **Every error the engine logs counts**, the scripts' errors included. A machine without an audio device logs a
  warning, not an error: the game runs on without sound.
- **The save folder is the player's own** (see [Saving](Saving.md)). On Linux, a test that must not touch it sets
  `XDG_DATA_HOME` to a folder of its own. On Windows, the folder is always in the roaming AppData: a test project gets
  its own `project_id`.
- **On Windows**, a release build is a windowed program. Started from a terminal with options, it writes its output
  to that terminal, but the terminal does not wait for it: `start /wait OvGame.exe ...` in `cmd`, or
  `Start-Process -Wait -PassThru` in PowerShell, waits and gives the exit code.
