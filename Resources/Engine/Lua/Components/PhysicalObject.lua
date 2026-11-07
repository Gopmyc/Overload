---@meta

--- Base class for any entity that is physical
---@class PhysicalObject : Component
PhysicalObject = {}

--- Returns the actor that owns this component
---@return Actor
function PhysicalObject:GetOwner() end

--- Returns the mass of the physical object
---@return number
function PhysicalObject:GetMass() end

--- Defines a new mass for the physical object
---@param mass number
function PhysicalObject:SetMass(mass) end

--- Returns the friction of the physical object
---@return number
function PhysicalObject:GetFriction() end

--- Defines a new friction for the physical object
---@param friction number
function PhysicalObject:SetFriction(friction) end

--- Returns the bounciness of the physical object
---@return number
function PhysicalObject:GetBounciness() end

--- Defines a new bounciness for the physical object
---@param bounciness number
function PhysicalObject:SetBounciness(bounciness) end

--- Defines a new linear velocity for the physical object
---@param velocity Vector3
function PhysicalObject:SetLinearVelocity(velocity) end

--- Defines a new angular velocity for the physical object
---@param velocity Vector3
function PhysicalObject:SetAngularVelocity(velocity) end

--- Returns the linear velocity of the physical object
---@return Vector3
function PhysicalObject:GetLinearVelocity() end

--- Returns the angular velocity of the physical object
---@return Vector3
function PhysicalObject:GetAngularVelocity() end

--- Defines a new linear factor for the physical object
---@param factor Vector3
function PhysicalObject:SetLinearFactor(factor) end

--- Defines a new angular factor for the physical object
---@param factor Vector3
function PhysicalObject:SetAngularFactor(factor) end

--- Returns the linear factor of the physical object
---@return Vector3
function PhysicalObject:GetLinearFactor() end

--- Returns the angular factor of the physical object
---@return Vector3
function PhysicalObject:GetAngularFactor() end

--- Returns true if the physical object is a trigger
---@return boolean
function PhysicalObject:IsTrigger() end

--- Defines if the physical object should be a trigger or not
---@param trigger boolean
function PhysicalObject:SetTrigger(trigger) end

--- Add a force to the physical object
---@param force Vector3
function PhysicalObject:AddForce(force) end

--- Add an impulse to the physical object
---@param impulse Vector3
function PhysicalObject:AddImpulse(impulse) end

--- Clear forces
function PhysicalObject:ClearForces() end

--- Defines a new collision detection mode for the physical object
---@param mode CollisionDetectionMode
function PhysicalObject:SetCollisionDetectionMode(mode) end

--- Returns the collision detection mode of the physical object
---@return CollisionDetectionMode
function PhysicalObject:GetCollisionMode() end

--- Defines if the physical object should be kinematic or not
---@param kinematic boolean
function PhysicalObject:SetKinematic(kinematic) end

--- Returns true if the physical object is static: it never moves by itself, and its body only follows its transform
--- when the transform changes
---@return boolean
function PhysicalObject:IsStatic() end

--- Defines if the physical object should be static or not (a static object ignores the kinematic setting)
---@param static boolean
function PhysicalObject:SetStatic(static) end

--- Returns the collision layer of the physical object (0 to 15)
---@return integer
function PhysicalObject:GetCollisionLayer() end

--- Puts the physical object on a collision layer (0 to 15, clamped): which layers collide is set by the project
--- (`physics_ignored_layer_pairs`) and by `Physics.SetLayersCollide`
---@param layer integer
function PhysicalObject:SetCollisionLayer(layer) end
