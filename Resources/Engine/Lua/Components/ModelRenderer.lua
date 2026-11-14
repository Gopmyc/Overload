---@meta

--- A ModelRenderer is necessary in combination with a MaterialRenderer to render a model in the world
---@class ModelRenderer : Component
ModelRenderer = {}

--- Returns the actor that owns this component
---@return Actor
function ModelRenderer:GetOwner() end

--- Returns the current model
---@return Model|nil
function ModelRenderer:GetModel() end

--- Defines the model to use
---@param model Model
function ModelRenderer:SetModel(model) end

--- Returns the current bounding mode
---@return FrustumBehaviour
function ModelRenderer:GetFrustumBehaviour() end

--- Sets a bounding mode
---@param behaviour FrustumBehaviour
function ModelRenderer:SetFrustumBehaviour(behaviour) end

--- Returns the distance from the camera beyond which the model isn't drawn (0: always drawn)
---@return number
function ModelRenderer:GetMaxDrawDistance() end

--- Sets the distance from the camera beyond which the model isn't drawn, in any pass: rendering, shadows,
--- reflections and picking (0: always drawn). Measured from the actor's position.
---@param distance number
function ModelRenderer:SetMaxDrawDistance(distance) end

--- Sets the lighter levels standing for the model from a distance on, each a table { model, distance }, the distances
--- rising (at most 7 levels). A level is drawn from its distance on, measured from the camera to the centre of the
--- model's bounds, with the renderer's materials by slot; beyond the last one, the max draw distance keeps its role.
--- A model with bones takes levels with the same bones, in the same order. Raises an error on a level refused.
---@param levels { [1]: Model, [2]: number }[]
function ModelRenderer:SetLodModels(levels) end

--- Returns the lighter levels, each a table { model, distance }
---@return { [1]: Model, [2]: number }[]
function ModelRenderer:GetLodModels() end

--- Returns the level the scene's main camera drew last: 0 for the model itself, n for the nth lighter level
---@return integer
function ModelRenderer:GetLodLevel() end

--- Returns why a level is left out (no model, not beyond the level before, other bones), or nil
---@return string|nil
function ModelRenderer:GetLodError() end

--- Sets the factor of every level's distance: above 1 keeps the detail further away, below 1 lets it go nearer
---@param bias number above 0
function ModelRenderer:SetLodBias(bias) end

--- Returns the factor of every level's distance (1 by default)
---@return number
function ModelRenderer:GetLodBias() end
