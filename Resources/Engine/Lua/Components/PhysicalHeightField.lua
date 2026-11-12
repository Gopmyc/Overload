---@meta

--- Represents a static physical object shaped by a square grid of heights. The sample (x, z) stands at
--- (x * spacing, height, z * spacing) from the actor, and each cell is split along its (x, z)-(x + 1, z + 1) diagonal
---@class PhysicalHeightField : PhysicalObject
PhysicalHeightField = {}

--- Defines the heights of the field, row-major: along +X first, then +Z (the sample (x, z) is at index
--- z * sampleCount + x + 1). A height of math.huge is a hole: no triangle touching that sample collides. An empty
--- array clears the field, which then collides with nothing. Returns true, or false and why when an argument is
--- invalid (nothing is changed then)
---@param sampleCount integer samples per side, from 3 to 4096
---@param spacing number distance between two neighbouring samples
---@param heights number[] sampleCount * sampleCount heights (math.huge for a hole), or none
---@return boolean
---@return string|nil
function PhysicalHeightField:SetHeights(sampleCount, spacing, heights) end

--- Returns the number of samples per side
---@return integer
function PhysicalHeightField:GetSampleCount() end

--- Returns the distance between two neighbouring samples
---@return number
function PhysicalHeightField:GetSpacing() end
