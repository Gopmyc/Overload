---@meta

--- Swings chains of bones of its actor's skinned mesh (hair, a cape, a tail) as the body moves, over the animation.
--- Chains are put in groups, each with its settings and the colliders it meets; a chain starts at a bone and runs down
--- its only child bone after bone. Colliders are capsules on the body's bones; the ground can be a plane at the
--- actor's height. Applied once a frame, after every script and every LookAt, before the bone attachments. A model no
--- render pass drew rests and does no work; back in view, or after a jump further than a run would make (a teleport, a
--- respawn), its chains start again from rest.
---
--- Settings: stiffness (how hard a joint is drawn back to its rest, 1/s², 150), damping (how fast its swing dies, 1/s,
--- 8), gravity (a share of 9.81 m/s², what the rest already bears left out, 0.3), radius (a joint's thickness, metres,
--- 0.025), maxAngle (the most a segment strays from its rest, degrees, 45), link (how firmly neighbouring chains keep
--- their spacing, 0 to 1, 0)
---@class SpringBones : Component
SpringBones = {}

--- Returns the actor that owns this component
---@return Actor
function SpringBones:GetOwner() end

--- Adds a group, or changes its settings: the keys given change, the others keep their values (or the defaults)
---@param name string
---@param settings? table
function SpringBones:SetGroup(name, settings) end

--- Returns a group's settings, or nil when there is no group of that name
---@param name string
---@return table|nil
function SpringBones:GetGroupSettings(name) end

--- Removes a group, returning false when there is none of that name
---@param name string
---@return boolean
function SpringBones:RemoveGroup(name) end

--- Returns the names of the groups, in their order
---@return string[]
function SpringBones:GetGroupNames() end

--- Sets the chains of a group, each named by the bone it starts at, side by side in that order (a cape's columns, for
--- the link). Returns false when there is no group of that name
---@param group string
---@param chains string[]
---@return boolean
function SpringBones:SetGroupChains(group, chains) end

--- Returns the chains of a group, or nil when there is no group of that name
---@param group string
---@return string[]|nil
function SpringBones:GetGroupChains(group) end

--- Sets the colliders a group meets, by name. Returns false when there is no group of that name
---@param group string
---@param colliders string[]
---@return boolean
function SpringBones:SetGroupColliders(group, colliders) end

--- Returns the colliders a group meets, or nil when there is no group of that name
---@param group string
---@return string[]|nil
function SpringBones:GetGroupColliders(group) end

--- Adds a collider, or replaces the one of that name: a capsule { from = bone, to = bone, radius = metres }, or one from a
--- bone outwards, away from another, between two distances past it { from, away, start, end, radius }
---@param name string
---@param collider table
function SpringBones:SetCollider(name, collider) end

--- Removes a collider, returning false when there is none of that name
---@param name string
---@return boolean
function SpringBones:RemoveCollider(name) end

--- Returns the names of the colliders
---@return string[]
function SpringBones:GetColliderNames() end

--- Sets whether the chains stay above a ground at the actor's height plus an offset, metres
---@param enabled boolean
---@param offset? number
function SpringBones:SetGround(enabled, offset) end

--- Returns true when the chains stay above the ground
---@return boolean
function SpringBones:IsGroundEnabled() end

--- Returns the ground's offset from the actor's height
---@return number
function SpringBones:GetGroundOffset() end

--- Sets whether a model no render pass drew the frame before rests (true by default)
---@param value boolean
function SpringBones:SetSleepWhenHidden(value) end

--- Returns true when a model no render pass drew rests
---@return boolean
function SpringBones:GetSleepWhenHidden() end

--- Puts the chains back at rest from the next frame
function SpringBones:Reset() end

--- Returns true while the chains rest because the model was not drawn
---@return boolean
function SpringBones:IsAsleep() end

--- Returns how many chains are simulated (0 until the scene first applies the component)
---@return integer
function SpringBones:GetChainCount() end

--- Returns the end of a chain's bone in the world, as simulated: chains and bones from 0, chains over every group in
--- their order. Nil for an unknown chain or bone
---@param chain integer
---@param bone integer
---@return Vector3|nil
function SpringBones:GetJointPosition(chain, bone) end

--- Returns why a chain or a collider cannot be made (an unknown bone, a chain of one bone), or nil
---@return string|nil
function SpringBones:GetError() end
