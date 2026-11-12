--- The engine's standard library for Lua: the building blocks most games write for themselves.
---
--- ```lua
--- local Std = require(":Libraries/Std/Init")      -- or Resources.GetScript(":Libraries/Std/Init.lua")
--- local Timers = require(":Libraries/Std/Timers") -- a module on its own
--- ```
---
--- - Class: classes with single inheritance, instances, final names;
--- - Signal: an object's events, with listeners that end with their owner;
--- - Timers: delays and repeats, on game or real time, advancing by themselves;
--- - Tween: values carried over time, eased, chained, advancing by themselves;
--- - Ease: easing curves, and a frame rate independent approach;
--- - Pool: objects made once and taken again;
--- - StateMachine: states tried in order, with enter, update and exit.
---
--- See Docs/StandardLibrary.md in the engine's repository.
---@class Std
---@field Class StdClass
---@field Signal StdSignalModule
---@field Timers StdTimers
---@field Tween StdTweenModule
---@field Ease StdEase
---@field Pool StdPoolModule
---@field StateMachine StdStateMachineModule
local Std = {
	Class = require(":Libraries/Std/Class"),
	Signal = require(":Libraries/Std/Signal"),
	Timers = require(":Libraries/Std/Timers"),
	Tween = require(":Libraries/Std/Tween"),
	Ease = require(":Libraries/Std/Ease"),
	Pool = require(":Libraries/Std/Pool"),
	StateMachine = require(":Libraries/Std/StateMachine")
}

return Std
