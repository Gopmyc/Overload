---@meta

--- Some global physics functions
---@class Physics
Physics = {}

--- Casts a ray against all Physical Object in the Scene and returns information on what was hit (in no particular
--- order, triggers included, without distances). Physics.CastRay tells more
---@param origin Vector3
---@param direction Vector3
---@param distance number
---@return RaycastHit
function Physics.Raycast(origin, direction, distance) end

--- Casts a ray and returns the objects it meets, nearest first, each once: an empty table when it meets none. A ray
--- that starts inside an object meets it at distance 0
---@param origin Vector3
---@param direction Vector3 need not be normalized
---@param distance number
---@param layerMask? integer the layers seen (Physics.GetLayerMask), every layer when nil
---@param includeTriggers? boolean false when nil
---@return PhysicsHit[]
function Physics.CastRay(origin, direction, distance, layerMask, includeTriggers) end

--- Sweeps a sphere and returns the objects it meets, nearest first, each once. An object the sphere overlaps at its
--- origin is met at distance 0
---@param origin Vector3
---@param radius number
---@param direction Vector3 need not be normalized
---@param distance number
---@param layerMask? integer the layers seen (Physics.GetLayerMask), every layer when nil
---@param includeTriggers? boolean false when nil
---@return PhysicsHit[]
function Physics.CastSphere(origin, radius, direction, distance, layerMask, includeTriggers) end

--- Returns the physical objects that overlap a sphere, each once, in no particular order
---@param center Vector3
---@param radius number
---@param layerMask? integer the layers seen (Physics.GetLayerMask), every layer when nil
---@param includeTriggers? boolean false when nil
---@return PhysicalObject[]
function Physics.OverlapSphere(center, radius, layerMask, includeTriggers) end

--- Returns the physical objects that overlap a box, each once, in no particular order
---@param center Vector3
---@param halfExtents Vector3
---@param rotation? Quaternion none when nil
---@param layerMask? integer the layers seen (Physics.GetLayerMask), every layer when nil
---@param includeTriggers? boolean false when nil
---@return PhysicalObject[]
function Physics.OverlapBox(center, halfExtents, rotation, layerMask, includeTriggers) end

--- Returns the mask of the given layers, each a name or an index: Physics.GetLayerMask("Default", "Enemy")
---@param ... string|integer
---@return integer
function Physics.GetLayerMask(...) end

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
