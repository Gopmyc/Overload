--- Objects made once and taken again, such as projectiles or effects: Take gives a free one first, else makes a new
--- one while there are fewer than the maximum, else takes back the one taken the longest ago, so the newest always
--- shows.
---
--- ```lua
--- local Std = require(":Libraries/Std/Init")
---
--- local sparks = Std.Pool.New({
---     max = 32,
---     make = function() return Prefabs.Spawn("Prefabs/Spark.ovprefab") end,
---     isFree = function(spark) return not spark:IsActive() end,
---     evict = function(spark) spark:SetActive(false) end
--- })
--- local spark = sparks:Take()
--- ```
---
--- The spec of New: `max` (at least 1), `make()` (a new object, or nil when none can be made: the pool then makes no
--- more), `isFree(object)`, and optionally `canEvict(object)` (whether a busy one may be taken back, all of them by
--- default) and `evict(object)` (called as it is taken back).
---@class StdPoolModule
local Pool = {}

---@class StdPool
local Instance = {}
Instance.__index = Instance

--- Returns a new, empty pool
---@param spec {max: integer, make: fun(): any, isFree: fun(object: any): boolean, canEvict: (fun(object: any): boolean)?, evict: fun(object: any)?}
---@return StdPool
function Pool.New(spec)
	if type(spec) ~= "table" then
		error("Std.Pool: New takes a table", 2)
	end

	for _, key in ipairs({ "make", "isFree" }) do
		if type(spec[key]) ~= "function" then
			error("Std.Pool: '" .. key .. "' is a function", 2)
		end
	end

	for _, key in ipairs({ "canEvict", "evict" }) do
		if spec[key] ~= nil and type(spec[key]) ~= "function" then
			error("Std.Pool: '" .. key .. "' is a function", 2)
		end
	end

	return setmetatable({
		m_max = math.max(math.floor(type(spec.max) == "number" and spec.max or 1), 1),
		m_make = spec.make,
		m_isFree = spec.isFree,
		m_canEvict = spec.canEvict,
		m_evict = spec.evict,
		m_items = {},
		m_takeIndices = {},
		m_takeCount = 0,
		m_spent = false
	}, Instance)
end

--- Returns a free object, a new one, or the one taken the longest ago that may be taken back; nil when there is
--- none
---@return any
function Instance:Take()
	local taken, oldest = nil, nil

	for _, item in ipairs(self.m_items) do
		if self.m_isFree(item) then
			taken = item
			break
		end

		if (self.m_canEvict == nil or self.m_canEvict(item)) and
			(oldest == nil or self.m_takeIndices[item] < self.m_takeIndices[oldest]) then
			oldest = item
		end
	end

	if taken == nil and not self.m_spent and #self.m_items < self.m_max then
		taken = self.m_make()

		if taken == nil then
			self.m_spent = true
		else
			self.m_items[#self.m_items + 1] = taken
		end
	end

	if taken == nil and oldest ~= nil then
		if self.m_evict ~= nil then
			self.m_evict(oldest)
		end

		taken = oldest
	end

	if taken == nil then
		return nil
	end

	self.m_takeCount = self.m_takeCount + 1
	self.m_takeIndices[taken] = self.m_takeCount
	return taken
end

--- Returns the objects made so far, in the order they were made (the pool's own list: don't change it)
---@return any[]
function Instance:GetItems()
	return self.m_items
end

--- Returns how many objects were made so far
---@return integer
function Instance:GetCount()
	return #self.m_items
end

--- Returns how many takes there were so far
---@return integer
function Instance:GetTakeCount()
	return self.m_takeCount
end

--- Returns the number of the take that last gave an object (1 for the first take), or nil if it was never taken
---@param item any
---@return integer|nil
function Instance:GetTakeIndex(item)
	return self.m_takeIndices[item]
end

return Pool
