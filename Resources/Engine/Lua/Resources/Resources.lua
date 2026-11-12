---@meta

--- Some global resources functions
---@class Resources
Resources = {}

--- Loads (If not already loaded) and returns the Model identified by the given path. Returns nil on failure
---@param path string
---@return Model|nil
function Resources.GetModel(path) end

--- Loads (If not already loaded) and returns the Shader identified by the given path. Returns nil on failure
---@param path string
---@return Shader|nil
function Resources.GetShader(path) end

--- Loads (If not already loaded) and returns the Texture identified by the given path. Returns nil on failure
---@param path string
---@return Texture|nil
function Resources.GetTexture(path) end

--- Loads (If not already loaded) and returns the Material identified by the given path. Returns nil on failure
---@param path string
---@return Material|nil
function Resources.GetMaterial(path) end

--- Loads (If not already loaded) and returns the Sound identified by the given path. Returns nil on failure
---@param path string
---@return Sound|nil
function Resources.GetSound(path) end

--- Builds a model of one mesh from flat arrays of numbers and registers it under the given path, which needs no file
--- (Resources.GetModel(path) returns it too). Building again at the same path replaces the model's mesh in place: every
--- renderer using it draws the new one. Returns the model, or nil and why (an invalid array, or a model loaded from a
--- file at this path)
--- geometry.Positions: x, y, z per vertex (required)
--- geometry.Indices: three vertex indices per triangle, from 0, counter-clockwise seen from the front (required)
--- geometry.Normals: x, y, z per vertex (smoothed from the triangles when missing)
--- geometry.TexCoords: u, v per vertex (zero when missing)
--- geometry.Tangents, geometry.Bitangents: x, y, z per vertex (zero when missing)
--- or geometry is a MeshData (DistanceField:Mesh), used as it is
---@param path string
---@param geometry {Positions: number[], Indices: integer[], Normals: number[]|nil, TexCoords: number[]|nil, Tangents: number[]|nil, Bitangents: number[]|nil}|MeshData
---@param materialName string|nil name of the model's material slot (default "Material")
---@return Model|nil
---@return string|nil
function Resources.CreateModel(path, geometry, materialName) end

--- Loads (If not already loaded) and returns the value returned by the script identified by the given path. Returns nil on failure
--- The returned value is shared by every script. It is discarded whenever the script engine reloads, which happens when entering play mode, when refreshing scripts, and when a Behaviour is removed
---@param path string
---@return any
function Resources.GetScript(path) end
