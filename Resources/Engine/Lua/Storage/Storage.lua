---@meta

--- The one folder where scripts keep what must outlive a session (saved games, settings): "Saves" in the
--- application's folder of the user's data (%APPDATA%\<executable_name>\Saves on Windows,
--- $XDG_DATA_HOME/<executable_name>/Saves, by default ~/.local/share, on Linux), shared by the built game and the
--- editor's play mode.
---
--- A script names a file by a relative path such as "slots/1.json", and nothing else can be reached:
--- - a path has 1 to 4 names separated by "/", 255 bytes at most; a name is 1 to 64 lower case letters, digits, "_"
---   and "-", in parts joined by single dots, and never a name Windows reserves ("con", "nul", "com1"...): no "..",
---   no absolute path, no drive, no backslash;
--- - a symbolic link or a junction found along the way is refused, so nothing leads out of the folder;
--- - a file holds 4 MiB at most, and the folder 64 MiB and 1024 entries;
--- - a file is written whole or not at all, so a crash in the middle leaves the previous version;
--- - what is read comes back as a string, never run.
---
--- A function never raises an error: it returns nil and what went wrong ("invalid path", "unsafe path", "not found",
--- "not a file", "not a folder", "folder not empty", "file too large", "storage full", "invalid data",
--- "io error: ..."). The files stay the player's own, readable and editable: this guards the player's computer from the
--- scripts, not the saves from the player.
---@class Storage
Storage = {}

--- Writes the whole file, making the folders that lead to it
---@param path string
---@param data string
---@return boolean|nil ok
---@return string|nil error
function Storage.Write(path, data) end

--- The whole content of a file
---@param path string
---@return string|nil data
---@return string|nil error
function Storage.Read(path) end

--- Whether a file or a folder is there
---@param path string
---@return boolean
function Storage.Exists(path) end

--- Removes a file, or a folder once it is empty
---@param path string
---@return boolean|nil ok
---@return string|nil error
function Storage.Delete(path) end

--- The names in a folder (the storage itself without a path), sorted, a folder's name ending with "/"
---@param path? string
---@return string[]|nil names
---@return string|nil error
function Storage.List(path) end
