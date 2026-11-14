---@meta

--- The settings of a vegetation layer, each optional (the layer keeps or takes its default)
---@class VegetationLayerSettings
---@field name? string
---@field model? Model the model scattered
---@field material? Material drawing every instance, its shader declaring INSTANCING (":Materials/Default.ovmat" by default)
---@field density? number instances per square metre where the density map weighs 1, from 0 to 100 (1 by default)
---@field minScale? number (1 by default)
---@field maxScale? number (1 by default)
---@field randomYaw? boolean a random turn about the vertical (true by default)
---@field slopeAlignment? number 0: upright, 1: along the ground's normal (0 by default)
---@field maxSlope? number degrees from the horizontal beyond which nothing stands, from 0 to 90 (90 by default)
---@field minHeight? number below which nothing stands, in the field's space (-10000 by default)
---@field maxHeight? number above which nothing stands, in the field's space (10000 by default)
---@field seed? integer the same seed scatters the same (0 by default)
---@field drawDistance? number from the camera to a chunk, beyond which its instances are not drawn (60 by default)
---@field castShadows? boolean (false by default)
---@field densityMap? string an image of the project, laid like the height map, its grey weighing each place from 0 to 1

--- Covers the actor's PhysicalHeightField with models scattered in layers (grass, flowers, stones), each chunk of the
--- field and layer drawn as one instanced draw
---@class Vegetation : Component
Vegetation = {}

--- Returns the actor that owns this component
---@return Actor
function Vegetation:GetOwner() end

--- Adds a layer and returns its index (from 0). An unknown setting, or one of the wrong kind, raises an error, as
--- does a ninth layer
---@param settings? VegetationLayerSettings
---@return integer
function Vegetation:AddLayer(settings) end

--- Changes the settings given of a layer, the others kept: the layer is scattered again
---@param layer integer from 0
---@param settings VegetationLayerSettings
function Vegetation:SetLayer(layer, settings) end

--- Returns a layer's settings, or nil for an unknown layer
---@param layer integer from 0
---@return VegetationLayerSettings|nil
function Vegetation:GetLayer(layer) end

--- Removes a layer: the ones after it move down by one
---@param layer integer from 0
function Vegetation:RemoveLayer(layer) end

--- Returns the number of layers
---@return integer
function Vegetation:GetLayerCount() end

--- Sets a layer's density map from weights from 0 to 1, size × size, row-major along X then Z like the heights,
--- stretched over the whole field; the layer's density map image is left. A size of 0 clears the map. Returns true,
--- or false and why. Like heights from a script, these weights are not saved
---@param layer integer from 0
---@param size integer from 2 to 4096, or 0
---@param weights number[]
---@return boolean
---@return string|nil
function Vegetation:SetDensityMap(layer, size, weights) end

--- Returns what keeps the vegetation or a layer from being drawn (no height field, a layer without a model or with a
--- material that cannot draw instances, a density map that cannot be read), or nil
---@return string|nil
function Vegetation:GetError() end

--- Returns the instances in the chunks within reach, as the main camera saw them last (before the frustum culls whole
--- chunks), of every layer or of one
---@param layer? integer from 0
---@return integer
function Vegetation:GetInstanceCount(layer) end
