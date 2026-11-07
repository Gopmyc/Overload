---@meta

--- Where a ray or a cast sphere met a physical object
---@class PhysicsHit
---@field Object PhysicalObject The object met
---@field Point Vector3 The point met, in world space
---@field Normal Vector3 The object's surface normal at that point, pointing out of the object
---@field Distance number How far the cast went from its origin before meeting the object
PhysicsHit = {}
