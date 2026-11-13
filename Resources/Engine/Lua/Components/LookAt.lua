---@meta

--- Turns a chain of bones of its actor's skinned mesh (the spine, the neck, the head) towards a target, over the
--- animation: a yaw and a pitch in the model's space (front +Z, up +Y), within bounds, following the target on a
--- critically damped spring. Each bone takes its part of the look, from the root to the tip, so that the tip looks the
--- whole way. While it has a target, the look holds its direction in the world as the body turns under it; without
--- one, it goes back to the front. Applied once a frame, after every script and before the bone attachments.
--- Angles are in degrees, times in seconds
---@class LookAt : Component
LookAt = {}

--- Returns the actor that owns this component
---@return Actor
function LookAt:GetOwner() end

--- Sets the bones that turn and their parts of the look (taken over their sum), from the root to the tip:
--- { { "spine_02", 0.2 }, { "neck_01", 0.3 }, { "head", 0.5 } }. Each must hang under the one before
---@param bones table
function LookAt:SetBones(bones) end

--- Returns the bones that turn and their parts, as SetBones takes them
---@return table
function LookAt:GetBones() end

--- Looks at an actor (its position plus the target offset), or at nothing with nil
---@param target Actor|nil
function LookAt:SetTarget(target) end

--- Returns the actor looked at, if any
---@return Actor|nil
function LookAt:GetTarget() end

--- Looks at a point in the world (plus the target offset), instead of an actor
---@param position Vector3
function LookAt:SetTargetPosition(position) end

--- Returns the point looked at when no actor is, if any
---@return Vector3|nil
function LookAt:GetTargetPosition() end

--- Looks at nothing: the look goes back to the front
function LookAt:ClearTarget() end

--- Returns true while there is something to look at
---@return boolean
function LookAt:HasTarget() end

--- Sets an offset added to the target, world space (an actor's eyes above its feet)
---@param offset Vector3
function LookAt:SetTargetOffset(offset) end

--- Returns the target offset
---@return Vector3
function LookAt:GetTargetOffset() end

--- Sets how far the look turns either side of the front (70 by default, 0 to 180)
---@param degrees number
function LookAt:SetMaxYaw(degrees) end

--- Returns how far the look turns either side of the front
---@return number
function LookAt:GetMaxYaw() end

--- Sets how far the look turns down (-30 by default)
---@param degrees number
function LookAt:SetMinPitch(degrees) end

--- Returns how far the look turns down
---@return number
function LookAt:GetMinPitch() end

--- Sets how far the look turns up (40 by default)
---@param degrees number
function LookAt:SetMaxPitch(degrees) end

--- Returns how far the look turns up
---@return number
function LookAt:GetMaxPitch() end

--- Sets roughly how long a turn takes (0.22 by default; 0 turns at once)
---@param seconds number
function LookAt:SetTurnTime(seconds) end

--- Returns roughly how long a turn takes
---@return number
function LookAt:GetTurnTime() end

--- Sets the fastest the look turns, degrees a second (0 for no limit, the default)
---@param degreesPerSecond number
function LookAt:SetMaxTurnSpeed(degreesPerSecond) end

--- Returns the fastest the look turns
---@return number
function LookAt:GetMaxTurnSpeed() end

--- Sets how much of the look applies, 0 to 1, reached over the fade time
---@param weight number
function LookAt:SetWeight(weight) end

--- Returns the weight asked for
---@return number
function LookAt:GetWeight() end

--- Returns the weight applying now, on its way to the one asked for
---@return number
function LookAt:GetCurrentWeight() end

--- Sets how long the weight takes to go from 0 to 1 (0.3 by default)
---@param seconds number
function LookAt:SetFadeTime(seconds) end

--- Returns how long the weight takes to go from 0 to 1
---@return number
function LookAt:GetFadeTime() end

--- Returns the look's yaw, positive towards the model's +X (the weight applied)
---@return number
function LookAt:GetYaw() end

--- Returns the look's pitch, positive up (the weight applied)
---@return number
function LookAt:GetPitch() end

--- Returns the direction looked in, world space: where the eyes point
---@return Vector3
function LookAt:GetLookDirection() end

--- Returns true when the look is at the front and still
---@return boolean
function LookAt:IsAtRest() end

--- Puts the look back to the front at once
function LookAt:Reset() end

--- Returns why the look cannot apply (no skinned mesh, an unknown bone, bones that make no chain), or nil
---@return string|nil
function LookAt:GetError() end
