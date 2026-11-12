---@meta

--- A component is a set of data and behaviours that is interpreted by the engine
---@class Component
Component = {}

--- Returns the actor that owns this component
---@return Actor
function Component:GetOwner() end

--- Returns true while the component is on its actor. Lua holds components as handles: using one that was removed, or
--- whose actor was freed, raises an error, which this tests without raising
---@return boolean
function Component:IsAlive() end
