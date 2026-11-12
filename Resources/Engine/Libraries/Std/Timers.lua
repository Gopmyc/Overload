--- Delays and repeats, on game time (which follows Time.SetScale) or on real time. They advance by themselves once a
--- frame, after every Behaviour's OnLateUpdate: no Behaviour needs to drive them.
---
--- ```lua
--- local Std = require(":Libraries/Std/Init")
---
--- Std.Timers.After(2, function() door:Open() end, { owner = self })
--- local blink = Std.Timers.Every(0.5, function() light:Toggle() end)
--- blink:Cancel()
--- ```
---
--- - Options: `unscaled` (true: real time, which goes on while the game is paused) and `owner` (an actor, or a
---   Behaviour's table: the timer ends with that actor).
--- - A timer made during a frame counts that frame's step; one made while the timers advance counts from the next.
--- - Every fires at most once a frame: a frame longer than its period starts the next period from then, without
---   catching up. Its callback returning false stops it.
--- - A callback that raises an error is logged and its timer cancelled; the others go on.
--- - Timers outlive scenes, as modules do: give them an owner to end with a scene's actors.
---@class StdTimers
local Timers = {}

local Owner = require(":Libraries/Std/Owner")

-- Seconds: a timer this close to its end is due (steps added up drift by a rounding)
local kEpsilon = 1e-6

local active = {}
local added = {}
local driven = false

---@class StdTimer
local Timer = {}
Timer.__index = Timer

--- Cancels the timer: its callback is not called any more
function Timer:Cancel()
	self.m_cancelled = true
end

--- Returns true while the timer will still call its callback
---@return boolean
function Timer:IsPending()
	return not self.m_cancelled and not self.m_done
end

--- One step of a timer; returns whether it goes on
local function Step(timer, gameStep, realStep)
	if timer.m_cancelled or Owner.IsGone(timer.m_owner) then
		return false
	end

	timer.m_left = timer.m_left - math.max((timer.m_unscaled and realStep or gameStep) or 0, 0)

	if timer.m_left > kEpsilon then
		return true
	end

	local ok, result = pcall(timer.m_callback)

	if not ok then
		Debug.LogError("Std.Timers: a callback failed and its timer is cancelled: " .. tostring(result))
		return false
	end

	if timer.m_period == nil or result == false or timer.m_cancelled then
		return false
	end

	timer.m_left = timer.m_left + timer.m_period

	if timer.m_left <= kEpsilon then
		timer.m_left = timer.m_period
	end

	return true
end

local function Update(gameStep, realStep)
	local current = added
	added = {}

	for i = 1, #current do
		active[#active + 1] = current[i]
	end

	local kept = 0

	for i = 1, #active do
		local timer = active[i]
		active[i] = nil

		if Step(timer, gameStep, realStep) then
			kept = kept + 1
			active[kept] = timer
		else
			timer.m_done = true
		end
	end
end

--- Advances the timers once a frame, from the first one made on
local function Drive()
	if driven then
		return
	end

	driven = true

	Events.AddLateUpdate(function(deltaTime)
		local ok, message = pcall(Update, deltaTime, Time.GetUnscaledDeltaTime())

		if not ok then
			Debug.LogError("Std.Timers: " .. tostring(message))
		end
	end)
end

local function Add(delay, period, callback, options, functionName)
	if type(delay) ~= "number" then
		error("Std.Timers: " .. functionName .. " takes a number of seconds", 3)
	end

	if type(callback) ~= "function" then
		error("Std.Timers: " .. functionName .. " takes a function", 3)
	end

	if options ~= nil and type(options) ~= "table" then
		error("Std.Timers: the options are a table", 3)
	end

	options = options or {}
	Owner.Check(options.owner, "Std.Timers", 3)

	local timer = setmetatable({
		m_left = math.max(delay, 0),
		m_period = period,
		m_callback = callback,
		m_unscaled = options.unscaled == true,
		m_owner = options.owner,
		m_cancelled = false,
		m_done = false
	}, Timer)

	added[#added + 1] = timer
	Drive()
	return timer
end

--- Calls a function once, after a delay
---@param delay number seconds
---@param callback fun()
---@param options {unscaled: boolean?, owner: Actor|table|nil}|nil
---@return StdTimer
function Timers.After(delay, callback, options)
	return Add(delay, nil, callback, options, "After")
end

--- Calls a function every period, until it returns false or the timer is cancelled
---@param period number seconds
---@param callback fun(): boolean?
---@param options {unscaled: boolean?, owner: Actor|table|nil}|nil
---@return StdTimer
function Timers.Every(period, callback, options)
	if type(period) == "number" then
		period = math.max(period, 0)
	end

	return Add(period, period, callback, options, "Every")
end

--- Cancels every timer
function Timers.Clear()
	for _, timer in ipairs(active) do
		timer.m_done = true
	end

	for _, timer in ipairs(added) do
		timer.m_done = true
	end

	active = {}
	added = {}
end

return Timers
