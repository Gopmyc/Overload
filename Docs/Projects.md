# Projects

A project is a folder holding an `Assets` folder and a project file, `<name>.ovproject`: plain `key=value` lines that
the editor and the built game read.

| Key | Default | What it sets |
| --- | --- | --- |
| `executable_name` | `Game` | The built game's name and window title. It names the save folder when `project_id` is empty |
| `project_id` | empty | The name of the save folder (see [Saving](Saving.md)): set it once, and renaming the game keeps its saves |
| `start_scene` | empty | The scene a built game opens, relative to `Assets` |
| `x_resolution`, `y_resolution` | 1280, 720 | The window's size |
| `fullscreen` | `false` | A full-screen window |
| `resizable` | `false` | Whether the player may resize the window |
| `vsync` | `true` | Vertical synchronisation |
| `multisampling`, `samples` | `false`, 4 | Multisample anti-aliasing |
| `gravity` | -9.81 | The physics' gravity along Y |
| `physics_layer_names` | `Default` for 0, `Ragdoll` for 15 | The collision layers' names, comma-separated, from layer 0 (see [Physics](Physics.md)) |
| `physics_ignored_layer_pairs` | empty | The pairs of layers that go through each other, comma-separated, as `a:b` |
| `physics_ragdoll_layer` | 15 | The layer of the ragdolls' bodies, by name or index |
| `physics_ragdoll_ignores_dynamic_bodies` | `false` | Ragdolls go through dynamic bodies |
| `window_icon` | empty | The window's icon, relative to `Assets` (the engine's icon when empty) |
| `build_type` | the platform's | The editor's build configuration |

- When the editor opens a project whose file lacks a key, for instance one added by a newer version of the engine, it
  adds the key with its default and keeps every other value. The file is written back, its lines possibly in another
  order. The `physics_` keys are the exception: they are optional, the editor never adds them, and a missing one
  keeps its default.
- The editor's Project Settings panel edits these values; its Reset button sets them all back to their defaults.
