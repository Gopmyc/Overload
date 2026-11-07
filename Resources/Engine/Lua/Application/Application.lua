---@meta

--- The running application
---@class Application
Application = {}

--- Asks the application to quit: a built game closes its window once the current frame is over, the editor leaves
--- play mode
function Application.Quit() end

--- The version of the engine running the game, as "major.minor.patch" (VERSION.txt), so that a project can check
--- the version it was made for
---@return string
function Application.GetEngineVersion() end

--- The version of the Lua API the engine gives to scripts: the minor version grows when the API gains names, the
--- major version when a name deprecated earlier is removed or changes meaning (see the engine's Docs/Scripting.md).
--- A script made for 1.2 runs on any 1.x from 1.2 on.
---@return integer major
---@return integer minor
function Application.GetApiVersion() end
