---@meta

--- A character moved by its velocity rather than pushed by contacts: it stands on slopes it can walk without sliding,
--- climbs steps, keeps to the ground going down, and falls under gravity when nothing holds it. Its capsule is upright
--- and centred on its actor, as a PhysicalCapsule's; only the actor's position follows the physics.
---
--- `GetLinearVelocity` and `SetLinearVelocity` are the velocity it moves at: the horizontal part is the one it was
--- given, walls or not; the vertical part is the ground's while it stands and grows with gravity while it falls, a
--- ceiling ends a rise and something that holds it up ends a fall. `AddImpulse` changes the velocity by the impulse
--- over the mass, `AddForce` over the next physics step. The settings of a body (friction, bounciness, factors,
--- trigger, kinematic, static, collision mode) mean nothing to a character.
---@class CharacterController : PhysicalObject
CharacterController = {}

--- Returns the radius of the capsule
---@return number
function CharacterController:GetRadius() end

--- Sets the radius of the capsule
---@param radius number
function CharacterController:SetRadius(radius) end

--- Returns the height of the straight part of the capsule: the character is height + 2 x radius tall
---@return number
function CharacterController:GetHeight() end

--- Sets the height of the straight part of the capsule
---@param height number
function CharacterController:SetHeight(height) end

--- Returns the steepest ground the character stands on, in degrees
---@return number
function CharacterController:GetMaxSlopeAngle() end

--- Sets the steepest ground the character stands on, in degrees (0 to 90, 50 by default)
---@param degrees number
function CharacterController:SetMaxSlopeAngle(degrees) end

--- Returns the highest step the character climbs
---@return number
function CharacterController:GetStepHeight() end

--- Sets the highest step the character climbs while it walks (0.3 by default, 0 climbs none)
---@param height number
function CharacterController:SetStepHeight(height) end

--- Returns how far down the character follows the ground it walks off
---@return number
function CharacterController:GetGroundSnapDistance() end

--- Sets how far down the character follows the ground it walks off instead of leaving it (0.3 by default, 0 never)
---@param distance number
function CharacterController:SetGroundSnapDistance(distance) end

--- Returns the share of the world's gravity the character falls with
---@return number
function CharacterController:GetGravityFactor() end

--- Sets the share of the world's gravity the character falls with (1 by default, 0 floats)
---@param factor number
function CharacterController:SetGravityFactor(factor) end

--- Returns the strongest force the character pushes dynamic bodies with, in newtons
---@return number
function CharacterController:GetPushStrength() end

--- Sets the strongest force the character pushes dynamic bodies with, in newtons (100 by default)
---@param strength number
function CharacterController:SetPushStrength(strength) end

--- Returns how fast the character really moved during the last physics step: against a wall, 0 into it
---@return Vector3
function CharacterController:GetRealVelocity() end

--- Returns what holds the character since the last physics step
---@return CharacterGroundState
function CharacterController:GetGroundState() end

--- Returns true if the character stands on ground no steeper than its maximum slope
---@return boolean
function CharacterController:IsGrounded() end

--- Returns the normal of the ground the character touches, (0, 0, 0) in the air
---@return Vector3
function CharacterController:GetGroundNormal() end

--- Returns the point where the character touches the ground
---@return Vector3
function CharacterController:GetGroundPoint() end

--- Returns the velocity of the ground the character touches: a moving platform carries the character along
---@return Vector3
function CharacterController:GetGroundVelocity() end

--- Returns the physical object the character stands on, nil in the air or on a ragdoll
---@return PhysicalObject|nil
function CharacterController:GetGroundObject() end
