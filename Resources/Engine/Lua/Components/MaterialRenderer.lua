---@meta

--- A component that handles a material list, necessary for model rendering
---@class MaterialRenderer : Component
MaterialRenderer = {}

--- Returns the actor that owns this component
---@return Actor
function MaterialRenderer:GetOwner() end

--- Defines the material to use for the given index
---@param index integer
---@param material Material
function MaterialRenderer:SetMaterial(index, material) end

--- Defines an element of the user matrix
---@param row integer
---@param column integer
---@param value number
function MaterialRenderer:SetUserMatrixElement(row, column, value) end

--- Returns an element of the user matrix
---@param row integer
---@param column integer
---@return number
function MaterialRenderer:GetUserMatrixElement(row, column) end

--- Defines the passes the renderer is drawn in: the scene (GEOMETRY), the shadow maps (SHADOW) and the reflection
--- probes (REFLECTION), all of them by default
---@param flags VisibilityFlags|integer
function MaterialRenderer:SetVisibilityFlags(flags) end

--- Returns the passes the renderer is drawn in
---@return VisibilityFlags|integer
function MaterialRenderer:GetVisibilityFlags() end
