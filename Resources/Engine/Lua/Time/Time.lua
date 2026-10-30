---@meta

--- The flow of game time
---@class Time
Time = {}

--- Returns the time scale: 1 is real time, below 1 slows the game down, 0 freezes it
---@return number
function Time.GetScale() end

--- Sets the time scale applied to the delta time of every update, physics and animation included (negative values are clamped to 0)
---@param scale number
function Time.SetScale(scale) end

--- Returns the duration of the last frame in seconds, regardless of the time scale
---@return number
function Time.GetUnscaledDeltaTime() end

--- Returns the seconds of game time since the start: they run at the time scale and stand still while it is 0. Shaders
--- read the same clock as ubo_GameTime, so a script can stamp a start time once and leave the rest to a shader.
---@return number
function Time.GetGameTime() end
