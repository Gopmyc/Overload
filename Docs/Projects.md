# Projects

A project is a folder holding an `Assets` folder and a project file, `<name>.ovproject`: plain `key=value` lines that
the editor and the built game read.

| Key | Default | What it sets |
| --- | --- | --- |
| `executable_name` | `Game` | The built game's name and window title. It also names the save folder (see [Saving](Saving.md)) |
| `start_scene` | empty | The scene a built game opens, relative to `Assets` |
| `x_resolution`, `y_resolution` | 1280, 720 | The window's size |
| `fullscreen` | `false` | A full-screen window |
| `resizable` | `false` | Whether the player may resize the window |
| `vsync` | `true` | Vertical synchronisation |
| `multisampling`, `samples` | `false`, 4 | Multisample anti-aliasing |
| `gravity` | -9.81 | The physics' gravity along Y |
| `window_icon` | empty | The window's icon, relative to `Assets` (the engine's icon when empty) |
| `build_type` | the platform's | The editor's build configuration |

- When the editor opens a project whose file lacks a key, for instance one added by a newer version of the engine, it
  adds the key with its default and keeps every other value. The file is written back, its lines possibly in another
  order.
- The editor's Project Settings panel edits these values; its Reset button sets them all back to their defaults.
