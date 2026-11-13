---@meta

--- Draws the PhysicalHeightField of its actor as a terrain: square chunks, each at a level of detail chosen by its
--- distance to the camera, with skirts hiding the gaps between levels. Its colour comes from a colour map (an image of
--- the project, or a colour per sample a script gives) laid on the albedo of its material
---@class Terrain : Component
Terrain = {}

--- Returns the actor that owns this component
---@return Actor
function Terrain:GetOwner() end

--- Sets the number of cells along a side of a chunk (32 by default)
---@param cells integer from 1 to 256
function Terrain:SetChunkSize(cells) end

--- Returns the number of cells along a side of a chunk
---@return integer
function Terrain:GetChunkSize() end

--- Sets the number of levels of detail (4 by default), as many as a chunk's side can halve
---@param count integer from 1 to 4
function Terrain:SetLodCount(count) end

--- Returns the number of levels of detail asked for
---@return integer
function Terrain:GetLodCount() end

--- Sets the distance from the camera at which a chunk leaves its full detail; each next level starts twice as far
--- (50 by default)
---@param distance number
function Terrain:SetLodDistance(distance) end

--- Returns the distance at which a chunk leaves its full detail
---@return number
function Terrain:GetLodDistance() end

--- Sets the material the terrain is drawn with (":Materials/Terrain.ovmat" by default); its albedo map gets the colour
--- map, if any. An empty path brings the default back
---@param path string
function Terrain:SetMaterialPath(path) end

--- Returns the material the terrain is drawn with
---@return string
function Terrain:GetMaterialPath() end

--- Sets the colour map, an image of the project stretched over the terrain (its top row along the far edge, +Z, as
--- the height map). Takes the place of colours a script gave
---@param path string
function Terrain:SetColorMap(path) end

--- Returns the colour map
---@return string
function Terrain:GetColorMap() end

--- Sets a colour per sample, laid as the colour map: red, green and blue from 0 to 1, row-major along +X then +Z, as
--- the heights. An empty array removes them. Returns true, or false and why (nothing is changed then)
---@param sampleCount integer samples per side, from 1 to 4096
---@param colors number[] sampleCount * sampleCount * 3 values, or none
---@return boolean
---@return string|nil
function Terrain:SetColors(sampleCount, colors) end

--- Returns true when colours a script gave make the colour map
---@return boolean
function Terrain:HasColors() end

--- Returns why the terrain draws nothing (no height field on the actor, no heights, no material), or nil
---@return string|nil
function Terrain:GetError() end

--- Returns the number of chunks
---@return integer
function Terrain:GetChunkCount() end

--- Returns the level of detail the main camera drew a chunk at last (0 is the full detail), or nil for an unknown chunk
---@param chunk integer from 0, row-major along +X then +Z
---@return integer|nil
function Terrain:GetChunkLod(chunk) end

--- Returns the triangles of the surface at the levels the main camera drew last, every chunk counted (the skirts left
--- out)
---@return integer
function Terrain:GetTriangleCount() end
