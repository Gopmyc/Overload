--- A state machine whose states are tried in order: the first whose condition holds (or which has none) is the
--- state. The owner, whatever the states act on, is passed to each of their functions.
---
--- ```lua
--- local Std = require(":Libraries/Std/Init")
---
--- self.machine = Std.StateMachine.New(self, {
---     { name = "Flee", condition = function(mind) return mind.health < 10 end,
---       update = function(mind, dt) mind:Run(dt) end },
---     { name = "Fight", condition = function(mind) return mind.target ~= nil end,
---       enter = function(mind) mind:Draw() end },
---     { name = "Idle" }
--- })
--- self.machine:Update(deltaTime) -- in OnUpdate
--- ```
---
--- A state: `name`, and optionally `condition(owner)`, `enter(owner, previousName)`, `update(owner, deltaTime)` and
--- `exit(owner, nextName)`.
---@class StdStateMachineModule
local StateMachine = {}

---@class StdStateMachine
local Instance = {}
Instance.__index = Instance

--- Returns a new machine, in no state until its first Update or SetState
---@param owner any passed to the states' functions
---@param states table[] tried in order
---@return StdStateMachine
function StateMachine.New(owner, states)
	if type(states) ~= "table" or #states == 0 then
		error("Std.StateMachine: New takes a list of states", 2)
	end

	local byName = {}

	for index, state in ipairs(states) do
		if type(state) ~= "table" or type(state.name) ~= "string" then
			error("Std.StateMachine: state " .. index .. " has no name", 2)
		end

		if byName[state.name] then
			error("Std.StateMachine: two states are named '" .. state.name .. "'", 2)
		end

		for _, key in ipairs({ "condition", "enter", "update", "exit" }) do
			if state[key] ~= nil and type(state[key]) ~= "function" then
				error("Std.StateMachine: '" .. key .. "' of state '" .. state.name .. "' is a function", 2)
			end
		end

		byName[state.name] = state
	end

	return setmetatable({ m_owner = owner, m_states = states, m_byName = byName, m_state = nil }, Instance)
end

--- Returns the state that should be the current one: the first whose condition holds, or which has none (nil if
--- there is none)
---@return table|nil
function Instance:Resolve()
	for _, state in ipairs(self.m_states) do
		if state.condition == nil or state.condition(self.m_owner) then
			return state
		end
	end

	return nil
end

local function Switch(self, state)
	local previous = self.m_state

	if previous == state then
		return
	end

	if previous ~= nil and previous.exit ~= nil then
		previous.exit(self.m_owner, state and state.name)
	end

	self.m_state = state

	if state ~= nil and state.enter ~= nil then
		state.enter(self.m_owner, previous and previous.name)
	end
end

--- Moves to the state that should be the current one, then updates it. Returns its name
---@param deltaTime number
---@return string|nil
function Instance:Update(deltaTime)
	Switch(self, self:Resolve())

	local state = self.m_state

	if state ~= nil and state.update ~= nil then
		state.update(self.m_owner, deltaTime)
	end

	return state and state.name
end

--- Returns the name of the current state, or nil
---@return string|nil
function Instance:GetState()
	return self.m_state and self.m_state.name
end

--- Moves to a state now, whatever the conditions say (the next Update resolves them again). Returns false if there
--- is no state of that name
---@param name string
---@return boolean
function Instance:SetState(name)
	local state = self.m_byName[name]

	if state == nil then
		return false
	end

	Switch(self, state)
	return true
end

return StateMachine
