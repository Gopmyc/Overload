---@meta

--- The current scene's navigation mesh, built at run time from its static colliders, and the paths along it
---@class Navigation
Navigation = {}

--- Builds the navigation mesh of the current scene from its static colliders (height fields, boxes, spheres,
--- capsules; not triggers), replacing the one built before. The settings default to `agentRadius` 0.4,
--- `agentHeight` 1.8, `agentMaxClimb` 0.4, `agentMaxSlope` 45 (degrees), `cellSize` 0.2, `cellHeight` 0.1,
--- `tileSize` 48 (cells) and `layerMask` 65535 (every collision layer, see Physics.GetLayerMask). A key it does not
--- know raises an error. Returns true, or false and why not
---@param settings? { agentRadius?: number, agentHeight?: number, agentMaxClimb?: number, agentMaxSlope?: number, cellSize?: number, cellHeight?: number, tileSize?: integer, layerMask?: integer }
---@return boolean built
---@return string|nil error
function Navigation.Build(settings) end

--- Builds again the tiles of the navigation mesh that a box touches, from the static colliders there now: after a
--- door shuts or a wall falls. Returns true, or false and why not
---@param min Vector3
---@param max Vector3
---@return boolean built
---@return string|nil error
function Navigation.Rebuild(min, max) end

--- Removes the current scene's navigation mesh
function Navigation.Clear() end

--- Returns true once the current scene's navigation mesh is built
---@return boolean
function Navigation.IsBuilt() end

--- Returns the settings of the navigation mesh built (every key that Build takes)
---@return { agentRadius: number, agentHeight: number, agentMaxClimb: number, agentMaxSlope: number, cellSize: number, cellHeight: number, tileSize: integer, layerMask: integer }
function Navigation.GetSettings() end

--- Returns the polygons of the navigation mesh
---@return integer
function Navigation.GetPolygonCount() end

--- Returns the corners of the path from a point to another, each taken to the nearest point of the mesh, and
--- whether it reaches the end (false: it ends at the point nearest to it that can be reached). Returns nil and why
--- when there is no mesh, or a point is too far from it
---@param from Vector3
---@param to Vector3
---@return Vector3[]|nil corners
---@return boolean|string complete
function Navigation.FindPath(from, to) end

--- Returns the nearest point of the navigation mesh within a box around a point (half sizes, 2, 4 and 2 by
--- default), or nil
---@param point Vector3
---@param extents? Vector3
---@return Vector3|nil
function Navigation.FindNearestPoint(point, extents) end

--- Walks along the navigation mesh from a point towards another, as seen from above. Returns whether an edge stopped
--- it, where it stopped, and the edge's normal; or nil when the start is not near the mesh
---@param from Vector3
---@param to Vector3
---@return boolean|nil hit
---@return Vector3|nil point
---@return Vector3|nil normal
function Navigation.Raycast(from, to) end

--- Returns a point of the navigation mesh drawn at random, or, given a center and a radius, one within that
--- distance and reachable from it; nil when there is none
---@param center? Vector3
---@param radius? number
---@return Vector3|nil
function Navigation.GetRandomPoint(center, radius) end

--- Sets the seed of the random points: the same seed gives the same points
---@param seed integer
function Navigation.SetSeed(seed) end

--- Returns the navigation mesh's surface as triangles, in the form Resources.CreateModel takes, for drawing it
---@return { Positions: number[], Indices: integer[] }
function Navigation.GetDebugGeometry() end
