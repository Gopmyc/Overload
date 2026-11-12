---@meta

--- Represents a static physical object shaped by a mesh of triangles, in the actor's space. Each triangle collides
--- from its front only, the side its counter-clockwise winding faces: rays and bodies coming from behind pass through
---@class PhysicalMesh : PhysicalObject
PhysicalMesh = {}

--- Defines the triangles: three numbers a vertex (x, y, z) in positions, three vertex indices from 0 a triangle in
--- indices. Empty arrays clear the mesh, which then collides with nothing. Returns true, or false and why when an
--- argument is invalid (nothing is changed then)
---@param positions number[]
---@param indices integer[]
---@return boolean
---@return string|nil
function PhysicalMesh:SetTriangles(positions, indices) end

--- Returns the number of triangles
---@return integer
function PhysicalMesh:GetTriangleCount() end
