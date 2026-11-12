--- Classes with single inheritance: a class is a table of methods and defaults its instances read through.
---
--- ```lua
--- local Std = require(":Libraries/Std/Init")
---
--- local Weapon = Std.Class.New()
--- function Weapon.New(damage) return Std.Class.Make(Weapon, { damage = damage }) end
--- function Weapon:Hit(target) target:Hurt(self.damage) end
---
--- local Sword = Std.Class.New(Weapon)
--- function Sword.New() return Std.Class.Make(Sword, { damage = 12 }) end
---
--- Std.Class.IsA(Sword.New(), Weapon) -- true
--- ```
---@class StdClass
local Class = {}

local kFinals = "__stdFinals"

--- Returns a new class, deriving from base when one is given. The class's `Base` field is its base
---@param base table|nil
---@return table
function Class.New(base)
	if base ~= nil and type(base) ~= "table" then
		error("Std.Class: a base is a class", 2)
	end

	local class = {}
	class.__index = class
	class.Base = base

	if base == nil then
		return class
	end

	-- The names the bases made final, read through the chain
	local finals = base[kFinals]

	return setmetatable(class, {
		__index = base,
		__newindex = finals and function(target, key, value)
			if finals[key] then
				error("Std.Class: '" .. tostring(key) .. "' is final in a base class and can't be overridden", 2)
			end

			rawset(target, key, value)
		end or nil
	})
end

--- Returns an instance of a class, made of the given fields (an empty table by default)
---@param class table
---@param fields table|nil
---@return table
function Class.Make(class, fields)
	if type(class) ~= "table" or rawget(class, "__index") ~= class then
		error("Std.Class: Make takes a class made by Class.New", 2)
	end

	if fields ~= nil and type(fields) ~= "table" then
		error("Std.Class: the fields of an instance are a table", 2)
	end

	return setmetatable(fields or {}, class)
end

--- Returns true if the value is an instance of the class, or of a class deriving from it
---@param value any
---@param class table
---@return boolean
function Class.IsA(value, class)
	if type(value) ~= "table" then
		return false
	end

	local current = getmetatable(value)

	while type(current) == "table" do
		if current == class then
			return true
		end

		current = rawget(current, "Base")
	end

	return false
end

--- Makes names of a class final: a class derived from it afterwards raises an error where it sets one of them, so
--- that what the class relies on stays its own
---@param class table
---@param names string[]
---@return table class
function Class.Final(class, names)
	if type(class) ~= "table" or type(names) ~= "table" then
		error("Std.Class: Final takes a class and a list of names", 2)
	end

	local finals = {}

	for name in pairs(class[kFinals] or {}) do
		finals[name] = true
	end

	for _, name in ipairs(names) do
		finals[name] = true
	end

	rawset(class, kFinals, finals)
	return class
end

return Class
