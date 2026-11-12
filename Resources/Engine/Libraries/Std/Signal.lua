--- Signals: an object says something happened, and whoever connected is called with what it emits.
---
--- ```lua
--- local Std = require(":Libraries/Std/Init")
---
--- self.Damaged = Std.Signal.New()                         -- in the emitter
--- self.Damaged:Emit(amount, source)
---
--- health.Damaged:Connect(function(amount, source) ... end, self) -- in a listener, a Behaviour
--- ```
---
--- - Listeners are called in the order they connected. One connected during an Emit is first called at the next.
--- - Disconnect takes effect at once, even during an Emit.
--- - A listener given an owner (an actor, or a Behaviour's table) is dropped once that actor is no longer alive, and
---   skipped while it is switched off.
--- - A listener that raises an error is logged and disconnected; the others are still called.
---@class StdSignalModule
local Signal = {}

local Owner = require(":Libraries/Std/Owner")

---@class StdSignal
local Instance = {}
Instance.__index = Instance

--- Returns a new signal, with no listener
---@return StdSignal
function Signal.New()
	return setmetatable({ m_connections = {}, m_nextId = 0, m_emitting = 0 }, Instance)
end

--- Drops the disconnected listeners, once no Emit is running
local function Compact(self)
	if self.m_emitting > 0 then
		return
	end

	local kept = {}

	for _, connection in ipairs(self.m_connections) do
		if connection.connected then
			kept[#kept + 1] = connection
		end
	end

	self.m_connections = kept
end

--- Connects a listener, called with what Emit is given. Returns an id for Disconnect
---@param callback function
---@param owner Actor|table|nil an actor, or a Behaviour's table, whose end disconnects the listener
---@return integer
function Instance:Connect(callback, owner)
	if type(callback) ~= "function" then
		error("Std.Signal: Connect takes a function", 2)
	end

	Owner.Check(owner, "Std.Signal", 2)

	self.m_nextId = self.m_nextId + 1
	self.m_connections[#self.m_connections + 1] = {
		id = self.m_nextId, callback = callback, owner = owner, connected = true
	}
	return self.m_nextId
end

--- Disconnects a listener. Returns false if it was not connected
---@param id integer
---@return boolean
function Instance:Disconnect(id)
	for _, connection in ipairs(self.m_connections) do
		if connection.id == id and connection.connected then
			connection.connected = false
			Compact(self)
			return true
		end
	end

	return false
end

--- Calls every listener with the given values
---@param ... any
function Instance:Emit(...)
	local connections = self.m_connections
	local count = #connections

	self.m_emitting = self.m_emitting + 1

	for i = 1, count do
		local connection = connections[i]

		if connection.connected then
			if Owner.IsGone(connection.owner) then
				connection.connected = false
			elseif Owner.IsActive(connection.owner) then
				local ok, message = pcall(connection.callback, ...)

				if not ok then
					connection.connected = false
					Debug.LogError("Std.Signal: a listener failed and is disconnected: " .. tostring(message))
				end
			end
		end
	end

	self.m_emitting = self.m_emitting - 1
	Compact(self)
end

--- Returns how many listeners are connected, those whose owner is gone left out
---@return integer
function Instance:GetCount()
	local count = 0

	for _, connection in ipairs(self.m_connections) do
		if connection.connected and not Owner.IsGone(connection.owner) then
			count = count + 1
		end
	end

	return count
end

--- Disconnects every listener
function Instance:Clear()
	for _, connection in ipairs(self.m_connections) do
		connection.connected = false
	end

	Compact(self)
end

return Signal
