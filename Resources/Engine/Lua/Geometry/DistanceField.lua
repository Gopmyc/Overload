---@meta

--- A signed distance field of empty spaces carved in solid ground: negative where it is empty, positive in the solid,
--- about the distance to the surface between them. The empty spaces are tubes (squashed, cut flat at the bottom so that
--- a body walks on them) and ellipsoids (cut flat too), blended together and roughened by a fractal value noise;
--- pillars stay solid inside them. With a ground (a square grid of heights split as a physical height field is), the
--- empty spaces only count under a cover of solid, but at openings: discs where the cover lifts and the air above the
--- ground counts as empty too. Lights make each vertex's openness. Computed natively: build it once, then mesh it by
--- blocks and ask it where floors are.
---@class DistanceField
DistanceField = {}

--- Returns a new, empty field: solid everywhere
---@return DistanceField
function DistanceField.new() end

--- Defines the seed of the noise
---@param seed integer
function DistanceField:SetSeed(seed) end

--- Defines the noise roughening the surfaces: in full on the walls and ceilings, floorShare of it on the floors (within
--- a metre and a half above the nearest space's floor), and on the pillars too
---@param amplitude number metres at most
---@param frequency number waves a metre
---@param octaves integer|nil 1 to 6 (2)
---@param floorShare number|nil 0 to 1 (0.2)
function DistanceField:SetNoise(amplitude, frequency, octaves, floorShare) end

--- Defines over how many metres two empty spaces blend into one
---@param radius number
function DistanceField:SetBlend(radius) end

--- Adds an empty tube from one point to another; its height is squash times its width, and its floor is cut flat floor
--- of its half height below its axis
---@param from Vector3
---@param to Vector3
---@param fromRadius number
---@param toRadius number
---@param floor number|nil 0 to 1 (1: no cut)
---@param squash number|nil 0.1 to 1 (1: round)
function DistanceField:AddTube(from, to, fromRadius, toRadius, floor, squash) end

--- Adds an empty ellipsoid, its floor cut flat floor of its vertical radius below its centre
---@param centre Vector3
---@param radii Vector3
---@param floor number|nil 0 to 1 (1: no cut)
function DistanceField:AddEllipsoid(centre, radii, floor) end

--- Adds a solid vertical pillar, kept solid within the empty spaces
---@param base Vector3
---@param height number
---@param radius number
function DistanceField:AddPillar(base, height, radius) end

--- Defines the ground: count * count heights, row-major (+X then +Z), spacing apart from (originX, originZ). The empty
--- spaces only count cover metres below it, and the air above an opening counts sink metres below it. Returns true, or
--- false and why
---@param count integer
---@param spacing number
---@param originX number
---@param originZ number
---@param heights number[]
---@param cover number
---@param sink number
---@return boolean
---@return string|nil
function DistanceField:SetGround(count, spacing, originX, originZ, heights, cover, sink) end

--- Adds an opening: a disc of the ground where the cover lifts (fully within 70 % of its radius)
---@param x number
---@param z number
---@param radius number
function DistanceField:AddOpening(x, z, radius) end

--- Adds a light: a vertex's openness is 1 at a light, falling to 0 at its radius
---@param position Vector3
---@param radius number
function DistanceField:AddLight(position, radius) end

--- Returns the value of the field at a point: negative where it is empty
---@param point Vector3
---@return number
function DistanceField:GetValue(point) end

--- Returns the height of the floor under an empty point, within depth metres, or nil (the point is solid, or no floor
--- is so near)
---@param point Vector3
---@param depth number
---@return number|nil
function DistanceField:FindFloor(point, depth) end

--- Returns the height of the ground at (x, z), or nil without a ground
---@param x number
---@param z number
---@return number|nil
function DistanceField:GetGroundHeight(x, z) end

--- Makes the surface of the block of cells voxels a side, voxel metres each, whose first voxel is at (x, y, z) *
--- voxel: two neighbouring blocks meet without a crack, its triangles face the empty side, and over a ground none
--- stands above it. A vertex's texture coordinates are its depth under the ground and its openness. Returns the mesh
--- data and, with a sample spacing, about one point of the surface every sampleSpacing metres (x, y, z, nx, ny, nz
--- each, the normal towards the empty side); nil and nil when no surface crosses the block
---@param x integer
---@param y integer
---@param z integer
---@param cells integer
---@param voxel number
---@param sampleSpacing number|nil
---@return MeshData|nil
---@return number[]|nil
function DistanceField:Mesh(x, y, z, cells, voxel, sampleSpacing) end
