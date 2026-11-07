---@meta

--- Some global physics functions
---@class Physics
Physics = {}

--- Casts a ray against all Physical Object in the Scene and returns information on what was hit
---@param origin Vector3
---@param direction Vector3
---@param distance number
---@return RaycastHit
function Physics.Raycast(origin, direction, distance) end

--- Returns the index of the collision layer of that name (from the project's `physics_layer_names`), or nil
---@param name string
---@return integer|nil
function Physics.GetLayerIndex(name) end

--- Returns the name of a collision layer (0 to 15), empty when it has none
---@param layer integer
---@return string
function Physics.GetLayerName(layer) end

--- Returns true if the bodies of the two collision layers collide
---@param layer1 integer
---@param layer2 integer
---@return boolean
function Physics.DoLayersCollide(layer1, layer2) end

--- Makes the bodies of the two collision layers collide or go through each other, from the next physics step. In the
--- editor, stopping play puts back the project's settings
---@param layer1 integer
---@param layer2 integer
---@param collide boolean
function Physics.SetLayersCollide(layer1, layer2, collide) end
