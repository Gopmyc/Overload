# Saving

## The save folder

- Each project has one save folder in the player's data folder, named after the project's `project_id`, or its
  `executable_name` while `project_id` is empty: `%APPDATA%\<name>\Saves` on Windows, `$XDG_DATA_HOME/<name>/Saves` or
  `~/.local/share/<name>/Saves` on Linux. The editor's play mode shares it with the built game. It is made at the first
  write.
- The name keeps letters, digits, `_`, `-` and single spaces; a name left empty, or one Windows keeps for a device,
  becomes `Game`.
- Changing the name moves the folder: the saves of the old name stay behind. A project sets `project_id` once, so
  that renaming the game keeps its saves. To keep the saves of a project that had none, set it to the current
  `executable_name`.

## `Storage`

- `Write(path, text)` makes the folders it needs; `Read(path)`; `Exists(path)`; `Delete(path)`, a file or an empty
  folder; `List(folder)`, sorted, a folder's name ending in `/`, `List()` for the folder itself.
- Nothing raises: a failure gives `nil` and a short message (`"invalid path"`, `"unsafe path"`, `"not found"`,
  `"not a file"`, `"not a folder"`, `"folder not empty"`, `"file too large"`, `"storage full"`, `"io error: …"`).
- Names: 1 to 4 names joined by `/`, 255 bytes at most; each name 64 bytes at most of lower-case letters, digits, `_`
  and `-`, in parts joined by single dots (`settings.json`, `slots/1.json`). No `..`, no leading or trailing dot, no
  capital, no name Windows keeps for a device (`con`, `nul.json`, `com1`…).
- Limits: 4 MiB a file; 64 MiB and 1024 files and folders in all.
- What a script cannot do: read, write, list or delete anything outside the folder (every part of a path that exists
  must be a plain folder or a plain file, so a link or a junction is refused); fill the disk; run what it reads; leave
  half a file (a write goes to a temporary file, flushed, then renamed over the old one). The files stay the player's,
  who can read and edit them: it is a sandbox for scripts, not an anti-cheat.

## `Json`

- `Encode(value, pretty)` and `Decode(text)`; `nil` and `"json: …"` on failure, naming where in the table or the line and
  column in the text.
- A table with the keys 1 to n is a list, one with string keys an object, its keys sorted; an empty table is `[]`.
  Integers stay integers, and a float keeps a `.0`.
- Refused: keys of both kinds, holes, functions, infinite numbers, invalid UTF-8, nesting past 64, text past 4 MiB.
- Decoding is strict JSON: a `null` member of an object is left out; a `null` in a list, a repeated key, a byte order
  mark or anything after the value is refused.
