# Projects

A project is a folder holding an `Assets` folder and a project file, `<name>.ovproject`: plain `key=value` lines that
the editor and the built game read.

## Keys the editor writes

| Key | Default | What it sets |
| --- | --- | --- |
| `executable_name` | `Game` | The built game's name and window title. It names the save folder when `project_id` is empty |
| `project_id` | empty | The name of the save folder (see [Saving](Saving.md)): set it once, and renaming the game keeps its saves |
| `start_scene` | empty | The scene a built game opens, relative to `Assets` (`--scene` replaces it, see [Command line](CommandLine.md)) |
| `x_resolution`, `y_resolution` | 1280, 720 | The window's size |
| `fullscreen` | `false` | A full-screen window |
| `resizable` | `false` | Whether the player may resize the window |
| `vsync` | `true` | Vertical synchronisation |
| `multisampling`, `samples` | `false`, 4 | Multisample anti-aliasing |
| `gravity` | -9.81 | The physics' gravity along Y |
| `window_icon` | empty | The window's icon, relative to `Assets` (the engine's icon when empty) |
| `input_actions` | empty | The project's input actions, a JSON file relative to `Assets` (see [Input](Input.md)); none when empty |
| `translations` | empty | The project's translations, a folder of `<language>.json` tables relative to `Assets` (see [Localization](Localization.md)); none when empty |
| `default_language` | `en` | The language a text missing from the current one is taken from, and the first one when neither the player nor the system chose |
| `audio_buses` | `Music,Effects,Interface,Ambience` | The project's audio buses, comma-separated, 8 at most, each with a volume of its own beside the Master's (see [Audio](Audio.md)); none when empty. A name given twice, and `Master`, are left out with a warning |
| `build_type` | the platform's | The editor's build configuration |

## Optional keys

The editor never adds these: a missing one keeps its default, and a number out of range is logged and brought within
it.

| Key | Default | What it sets |
| --- | --- | --- |
| `physics_layer_names` | `Default` for 0, `Ragdoll` for 15 | The collision layers' names, comma-separated, from layer 0 (see [Physics](Physics.md)) |
| `physics_ignored_layer_pairs` | empty | The pairs of layers that go through each other, comma-separated, as `a:b` |
| `physics_ragdoll_layer` | 15 | The layer of the ragdolls' bodies, by name or index |
| `physics_ragdoll_ignores_dynamic_bodies` | `false` | Ragdolls go through dynamic bodies |
| `storage_max_file_mib` | 4 | The largest saved file, in MiB (1 to 1024) |
| `storage_max_total_mib` | 64 | The most the save folder holds, in MiB (`storage_max_file_mib` to 16384) |
| `storage_max_entries` | 1024 | The most files and folders the save folder holds (1 to 1000000) |
| `json_max_mib` | 4 | The longest text `Json` encodes or decodes, in MiB (1 to 1024) |
| `json_max_depth` | 64 | The deepest nesting `Json` accepts (1 to 256) |
| `max_frame_delta` | 0.1 | The longest step a frame hands the game, in seconds (0.001 to 1); see [Scripting](Scripting.md) |

## Editing

- When the editor opens a project whose file lacks a key, for instance one added by a newer version of the engine, it
  adds the key with its default and keeps every other value. The file is written back, its lines possibly in another
  order. It leaves the optional keys out.
- The editor's Project Settings panel edits these values; its Reset button sets them all back to their defaults.
