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
