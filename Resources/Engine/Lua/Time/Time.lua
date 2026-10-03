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
