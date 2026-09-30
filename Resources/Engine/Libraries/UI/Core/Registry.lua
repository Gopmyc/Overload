--- Panel classes registry: inheritance and instantiation, modeled after vgui.Register / vgui.Create
---@class UIRegistry
local Registry = {}

local classes = {}

--- Registers a panel class, inheriting from an already registered base class
---@param className string
---@param class table
---@param baseName string|nil
---@return table
function Registry.Register(className, class, baseName)
	assert(type(className) == "string", "UI: a panel class name must be a string")
	assert(type(class) == "table", "UI: panel class '" .. className .. "' must be a table")

	local base = nil

	if baseName ~= nil then
		base = classes[baseName]

		if not base then
			error("UI: unknown base panel '" .. tostring(baseName) .. "' for '" .. className .. "'", 2)
		end
	end

	class.ClassName = className
	class.BaseClass = base
	class.__index = class
	setmetatable(class, base)
	classes[className] = class

	return class
end

--- Returns the class table registered under the given name
---@param className string
---@return table|nil
function Registry.Get(className)
	return classes[className]
end

--- Creates a panel, then runs every Init of its class chain from the base class to the most derived one
---@param controller table
---@param className string
---@param parent table|nil
---@return table
function Registry.Create(controller, className, parent)
	local class = classes[className]

	if not class then
		error("UI: unknown panel class '" .. tostring(className) .. "'", 2)
	end

	local chain = {}
	local current = class

	while current do
		chain[#chain + 1] = current
		current = rawget(current, "BaseClass")
	end

	local panel = setmetatable({}, class)
	panel:Setup(controller, parent)

	local succeeded, message = pcall(function()
		for i = #chain, 1, -1 do
			local init = rawget(chain[i], "Init")

			if init then
				init(panel)
			end
		end
	end)

	if not succeeded then
		panel:Remove()
		error(message, 0)
	end

	return panel
end

return Registry
