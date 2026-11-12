--- Internal to the standard library: what an `owner` option is. An owner is an actor, or a Behaviour's table (its
--- `owner` actor is taken). What is given an owner ends once that actor is no longer alive, and a signal skips a
--- listener whose owner is switched off.
---@class StdOwner
local Owner = {}

--- The actor an owner stands for, or nil without an owner
---@param owner Actor|table|nil
---@return Actor|nil
function Owner.Resolve(owner)
	if type(owner) == "table" and owner.owner ~= nil then
		return owner.owner
	end

	return owner
end

--- Raises when a value is neither nil, an actor nor a Behaviour's table
---@param owner any
---@param moduleName string
---@param level integer the level of the error, as for error()
function Owner.Check(owner, moduleName, level)
	if owner == nil then
		return
	end

	local actor = Owner.Resolve(owner)
	local kind = type(actor)

	if (kind ~= "userdata" and kind ~= "table") or type(actor.IsAlive) ~= "function" then
		error(moduleName .. ": an owner is an actor, or a Behaviour's table", level + 1)
	end
end

--- True once the owner's actor is no longer alive (never without an owner)
---@param owner Actor|table|nil
---@return boolean
function Owner.IsGone(owner)
	local actor = Owner.Resolve(owner)
	return actor ~= nil and not actor:IsAlive()
end

--- True without an owner, or while the owner's actor is alive and active
---@param owner Actor|table|nil
---@return boolean
function Owner.IsActive(owner)
	local actor = Owner.Resolve(owner)
	return actor == nil or (actor:IsAlive() and actor:IsActive())
end

return Owner
