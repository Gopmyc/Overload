---@meta

--- Vertices and triangles made on the CPU (by DistanceField:Mesh), for Resources.CreateModel or PhysicalMesh:SetMesh,
--- without going through script tables
---@class MeshData
MeshData = {}

--- Returns the number of vertices
---@return integer
function MeshData:GetVertexCount() end

--- Returns the number of triangles
---@return integer
function MeshData:GetTriangleCount() end
