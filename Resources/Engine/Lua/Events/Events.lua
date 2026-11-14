---@meta

--- Frame events outside any Behaviour, for the modules that keep a clock of their own (timers, tweens, a registry):
--- they need no actor in the scene to be run. A callback receives the frame's step, as OnUpdate does (game time, scaled
--- by the time scale, at most the project's max_frame_delta). The callbacks live with the Lua state: they outlive a
--- scene change, and the editor drops them when play starts and stops. Since API 1.1; the game's end since 1.24.
---@class Events
Events = {}

--- Calls the function every frame before every Behaviour's OnUpdate, from the next frame on, until Events.Remove, an
--- error (logged, then the callback is removed) or the end of its owner, an actor, when one is given
---@param callback fun(deltaTime: number)
---@param owner? Actor
---@return integer id
function Events.AddUpdate(callback, owner) end

--- Calls the function every frame after every Behaviour's OnLateUpdate, as Events.AddUpdate does otherwise
---@param callback fun(deltaTime: number)
---@param owner? Actor
---@return integer id
function Events.AddLateUpdate(callback, owner) end

--- Calls the function once, with nothing, as the game ends, its scene still current: a built game quitting or closed,
--- the end of OvGame --frames, a game stopped in the editor or the editor closed during one. The last chance to write
--- what was kept for later. An error is logged and the other callbacks still run; one whose owner, an actor, is no
--- longer alive is skipped; one added while the game ends is not called. Since API 1.24
---@param callback fun()
---@param owner? Actor
---@return integer id
function Events.AddQuit(callback, owner) end

--- Removes a callback, at once even while its event runs, and returns whether it was there
---@param id integer
---@return boolean
function Events.Remove(id) end
