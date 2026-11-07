---@meta

--- A reference to an asset, which Model(), Texture(), Shader(), Material(), Sound() and Prefab() make to declare a
--- Behaviour property. The engine fills the path from the inspector or the scene file; a script reads it
---@class AssetRef
---@field path string The asset's path, relative to the project's assets ("" when none is set)
AssetRef = {}
