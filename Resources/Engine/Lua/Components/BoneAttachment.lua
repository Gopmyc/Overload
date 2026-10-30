---@meta

--- Keeps its actor on a bone (or any node) of a skinned mesh: a weapon in a hand, a flame on a head.
--- The skinned mesh is the target actor's SkinnedMeshRenderer when a target is set, else the nearest ancestor's.
--- Every frame, once every script has run, the actor is placed at the bone's pose times the offset:
--- no script has to read the bone. Attachments are placed parents first, so one can ride on another.
---@class BoneAttachment : Component
BoneAttachment = {}

--- Returns the actor that owns this component
---@return Actor
function BoneAttachment:GetOwner() end

--- Sets the bone (or node) to follow, by name
---@param bone string
function BoneAttachment:SetBone(bone) end

--- Returns the name of the bone (or node) followed
---@return string
function BoneAttachment:GetBone() end

--- Sets the offset from the bone, in the bone's space
---@param position Vector3
function BoneAttachment:SetOffsetPosition(position) end

--- Returns the offset from the bone, in the bone's space
---@return Vector3
function BoneAttachment:GetOffsetPosition() end

--- Sets the rotation applied after the bone's
---@param rotation Quaternion
function BoneAttachment:SetOffsetRotation(rotation) end

--- Returns the rotation applied after the bone's
---@return Quaternion
function BoneAttachment:GetOffsetRotation() end

--- Sets the scale applied after the bone's
---@param scale Vector3
function BoneAttachment:SetOffsetScale(scale) end

--- Returns the scale applied after the bone's
---@return Vector3
function BoneAttachment:GetOffsetScale() end

--- Sets the actor whose SkinnedMeshRenderer holds the bone. Nil goes back to the nearest ancestor with one
---@param target Actor|nil
function BoneAttachment:SetTarget(target) end

--- Returns the target actor, nil when the nearest ancestor with a SkinnedMeshRenderer is used
---@return Actor|nil
function BoneAttachment:GetTarget() end

--- Returns true if the last placement found its skinned mesh and its bone
---@return boolean
function BoneAttachment:IsAttached() end
