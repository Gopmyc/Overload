---@meta

--- Represents a static physical object shaped by a square grid of heights (a terrain), made from a height map image of
--- the project or from heights a script gives. The sample (x, z) stands at (x * spacing, height, z * spacing) from the
--- actor, and each cell is split along its (x, z)-(x + 1, z + 1) diagonal. A Terrain on the same actor draws it
---@class PhysicalHeightField : PhysicalObject
PhysicalHeightField = {}

--- Defines the heights of the field, row-major: along +X first, then +Z (the sample (x, z) is at index
--- z * sampleCount + x + 1). An empty array clears the field, which then collides with nothing. The field leaves its
--- height map, if any. Returns true, or false and why when an argument is invalid (nothing is changed then)
---@param sampleCount integer samples per side, from 3 to 4096
---@param spacing number distance between two neighbouring samples
---@param heights number[] sampleCount * sampleCount heights, or none
---@return boolean
---@return string|nil
function PhysicalHeightField:SetHeights(sampleCount, spacing, heights) end

--- Returns the heights, row-major along +X then +Z (empty before any)
---@return number[]
function PhysicalHeightField:GetHeights() end

--- Returns the number of samples per side
---@return integer
function PhysicalHeightField:GetSampleCount() end

--- Returns the distance between two neighbouring samples
---@return number
function PhysicalHeightField:GetSpacing() end

--- Sets the distance between two neighbouring samples, the heights kept
---@param spacing number above 0
function PhysicalHeightField:SetSpacing(spacing) end

--- Makes the field from a height map, a square image of the project (a PNG of 8 or 16 bits): a pixel per sample, its
--- columns along +X and its rows along +Z from the top, its grey level from black to white giving the height offset
--- to the height offset plus the height scale. An empty path leaves the height map. A map that cannot be read empties
--- the field, and GetHeightmapError says why
---@param path string
function PhysicalHeightField:SetHeightmap(path) end

--- Returns the height map the field is made from, or an empty string
---@return string
function PhysicalHeightField:GetHeightmap() end

--- Returns why the height map cannot be read, or nil
---@return string|nil
function PhysicalHeightField:GetHeightmapError() end

--- Sets the height of a white pixel of the height map above a black one (10 by default)
---@param scale number
function PhysicalHeightField:SetHeightScale(scale) end

--- Returns the height of a white pixel of the height map above a black one
---@return number
function PhysicalHeightField:GetHeightScale() end

--- Sets the height of a black pixel of the height map (0 by default)
---@param offset number
function PhysicalHeightField:SetHeightOffset(offset) end

--- Returns the height of a black pixel of the height map
---@return number
function PhysicalHeightField:GetHeightOffset() end

--- Returns the height (y) of the field's surface under a point of the world, on the triangle the physics uses, or nil
--- outside the field. Exact for an actor turned about Y only
---@param x number
---@param z number
---@return number|nil
function PhysicalHeightField:GetHeightAt(x, z) end

--- Returns the normal of the field's surface under a point of the world, or nil outside the field
---@param x number
---@param z number
---@return Vector3|nil
function PhysicalHeightField:GetNormalAt(x, z) end
