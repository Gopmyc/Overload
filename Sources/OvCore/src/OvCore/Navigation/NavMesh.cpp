/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <format>

#include <DetourCommon.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <DetourNavMeshQuery.h>
#include <Recast.h>

#include <OvCore/Navigation/NavMesh.h>

namespace
{
	using OvMaths::FVector3;

	// Detour's references hold a tile and a polygon in 22 bits, besides the salt
	constexpr int kReferenceBits = 22;
	constexpr int kMaxPathPolygons = 512;
	constexpr int kMaxPathCorners = 512;
	constexpr int kMaxQueryNodes = 4096;
	constexpr unsigned short kWalkableFlag = 1;
	constexpr int kRandomAttempts = 32;

	// The random points' generator: Detour takes a plain function, which reads the state of the mesh drawing
	thread_local uint64_t* t_randomState = nullptr;

	float NextRandom()
	{
		uint64_t& state = *t_randomState;
		state ^= state >> 12;
		state ^= state << 25;
		state ^= state >> 27;
		const uint64_t value = state * 0x2545F4914F6CDD1Dull;
		return static_cast<float>(value >> 40) / static_cast<float>(1u << 24);
	}

	uint32_t NextPowerOfTwo(uint32_t p_value)
	{
		uint32_t result = 1;
		while (result < p_value) result <<= 1;
		return result;
	}

	int Log2(uint32_t p_value)
	{
		int result = 0;
		while ((1u << (result + 1)) <= p_value) ++result;
		return result;
	}

	FVector3 ToVector(const float* p_values)
	{
		return { p_values[0], p_values[1], p_values[2] };
	}

	void ToArray(const FVector3& p_vector, float* p_values)
	{
		p_values[0] = p_vector.x;
		p_values[1] = p_vector.y;
		p_values[2] = p_vector.z;
	}

	/**
	* Frees Recast's objects at the end of a tile's build, whatever happens
	*/
	struct TileWork
	{
		rcHeightfield* solid = nullptr;
		rcCompactHeightfield* compact = nullptr;
		rcContourSet* contours = nullptr;
		rcPolyMesh* polygons = nullptr;
		rcPolyMeshDetail* detail = nullptr;

		~TileWork()
		{
			rcFreeHeightField(solid);
			rcFreeCompactHeightfield(compact);
			rcFreeContourSet(contours);
			rcFreePolyMesh(polygons);
			rcFreePolyMeshDetail(detail);
		}
	};
}

const OvMaths::FVector3 OvCore::Navigation::NavMesh::kDefaultExtents{ 2.0f, 4.0f, 2.0f };

void OvCore::Navigation::NavGeometry::AddTriangle(const OvMaths::FVector3& p_a, const OvMaths::FVector3& p_b, const OvMaths::FVector3& p_c)
{
	const int first = static_cast<int>(vertices.size() / 3);

	for (const auto* vertex : { &p_a, &p_b, &p_c })
	{
		vertices.push_back(vertex->x);
		vertices.push_back(vertex->y);
		vertices.push_back(vertex->z);
	}

	indices.push_back(first);
	indices.push_back(first + 1);
	indices.push_back(first + 2);
}

size_t OvCore::Navigation::NavGeometry::GetTriangleCount() const
{
	return indices.size() / 3;
}

struct OvCore::Navigation::NavMesh::Impl
{
	NavMeshSettings settings;
	dtNavMesh* mesh = nullptr;
	dtNavMeshQuery* query = nullptr;
	dtQueryFilter filter;
	float origin[3] = { 0.0f, 0.0f, 0.0f };
	float minY = 0.0f;
	float maxY = 0.0f;
	int tilesX = 0;
	int tilesZ = 0;
	uint64_t random = 0x9E3779B97F4A7C15ull;

	~Impl()
	{
		Release();
	}

	void Release()
	{
		dtFreeNavMeshQuery(query);
		dtFreeNavMesh(mesh);
		query = nullptr;
		mesh = nullptr;
		tilesX = tilesZ = 0;
	}

	float TileWidth() const
	{
		return static_cast<float>(settings.tileSize) * settings.cellSize;
	}

	int BorderCells() const
	{
		return static_cast<int>(std::ceil(settings.agentRadius / settings.cellSize)) + 3;
	}

	/**
	* The triangles of the geometry that each tile of a range overlaps, its border included
	*/
	std::vector<std::vector<int>> BinTriangles(const NavGeometry& p_geometry, int p_x0, int p_z0, int p_x1, int p_z1) const
	{
		const int width = p_x1 - p_x0 + 1;
		std::vector<std::vector<int>> bins(static_cast<size_t>(width) * static_cast<size_t>(p_z1 - p_z0 + 1));

		const float tile = TileWidth();
		const float border = static_cast<float>(BorderCells()) * settings.cellSize;
		const auto& v = p_geometry.vertices;
		const auto& indices = p_geometry.indices;

		for (size_t triangle = 0; triangle * 3 + 2 < indices.size(); ++triangle)
		{
			float lowX = FLT_MAX, lowZ = FLT_MAX, highX = -FLT_MAX, highZ = -FLT_MAX;

			for (int corner = 0; corner < 3; ++corner)
			{
				const int index = indices[triangle * 3 + corner];
				lowX = std::min(lowX, v[index * 3]);
				highX = std::max(highX, v[index * 3]);
				lowZ = std::min(lowZ, v[index * 3 + 2]);
				highZ = std::max(highZ, v[index * 3 + 2]);
			}

			const int x0 = std::max(p_x0, static_cast<int>(std::floor((lowX - border - origin[0]) / tile)));
			const int x1 = std::min(p_x1, static_cast<int>(std::floor((highX + border - origin[0]) / tile)));
			const int z0 = std::max(p_z0, static_cast<int>(std::floor((lowZ - border - origin[2]) / tile)));
			const int z1 = std::min(p_z1, static_cast<int>(std::floor((highZ + border - origin[2]) / tile)));

			for (int z = z0; z <= z1; ++z)
			{
				for (int x = x0; x <= x1; ++x)
				{
					bins[static_cast<size_t>(z - p_z0) * width + (x - p_x0)].push_back(static_cast<int>(triangle));
				}
			}
		}

		return bins;
	}

	/**
	* Builds a tile's data for Detour from the triangles that overlap it. Returns false when the tile holds no
	* walkable surface, which is no error
	*/
	bool BuildTile(const NavGeometry& p_geometry, const std::vector<int>& p_triangles, int p_x, int p_z, unsigned char*& p_data, int& p_size, std::string& p_error) const
	{
		p_data = nullptr;
		p_size = 0;

		if (p_triangles.empty())
		{
			return false;
		}

		const NavMeshSettings& s = settings;
		rcContext context(false);

		rcConfig config{};
		config.cs = s.cellSize;
		config.ch = s.cellHeight;
		config.walkableSlopeAngle = s.agentMaxSlope;
		config.walkableHeight = static_cast<int>(std::ceil(s.agentHeight / s.cellHeight));
		config.walkableClimb = static_cast<int>(std::floor(s.agentMaxClimb / s.cellHeight));
		config.walkableRadius = static_cast<int>(std::ceil(s.agentRadius / s.cellSize));
		config.maxEdgeLen = static_cast<int>(12.0f / s.cellSize);
		config.maxSimplificationError = 1.3f;
		config.minRegionArea = 8 * 8;
		config.mergeRegionArea = 20 * 20;
		config.maxVertsPerPoly = DT_VERTS_PER_POLYGON;
		config.tileSize = static_cast<int>(s.tileSize);
		config.borderSize = BorderCells();
		config.width = config.tileSize + config.borderSize * 2;
		config.height = config.tileSize + config.borderSize * 2;
		config.detailSampleDist = s.cellSize * 6.0f;
		config.detailSampleMaxError = s.cellHeight;

		const float tile = TileWidth();
		const float border = static_cast<float>(config.borderSize) * s.cellSize;
		config.bmin[0] = origin[0] + static_cast<float>(p_x) * tile - border;
		config.bmin[1] = minY;
		config.bmin[2] = origin[2] + static_cast<float>(p_z) * tile - border;
		config.bmax[0] = origin[0] + static_cast<float>(p_x + 1) * tile + border;
		config.bmax[1] = maxY;
		config.bmax[2] = origin[2] + static_cast<float>(p_z + 1) * tile + border;

		// The triangles of this tile, their indices into the whole geometry's vertices
		std::vector<int> indices;
		indices.reserve(p_triangles.size() * 3);
		for (const int triangle : p_triangles)
		{
			indices.push_back(p_geometry.indices[triangle * 3]);
			indices.push_back(p_geometry.indices[triangle * 3 + 1]);
			indices.push_back(p_geometry.indices[triangle * 3 + 2]);
		}

		const int triangleCount = static_cast<int>(p_triangles.size());
		const int vertexCount = static_cast<int>(p_geometry.vertices.size() / 3);
		std::vector<unsigned char> areas(p_triangles.size(), RC_NULL_AREA);

		TileWork work;
		work.solid = rcAllocHeightfield();

		if (!work.solid || !rcCreateHeightfield(&context, *work.solid, config.width, config.height, config.bmin, config.bmax, config.cs, config.ch))
		{
			p_error = "out of memory for a tile's height field";
			return false;
		}

		rcMarkWalkableTriangles(&context, config.walkableSlopeAngle, p_geometry.vertices.data(), vertexCount, indices.data(), triangleCount, areas.data());

		if (!rcRasterizeTriangles(&context, p_geometry.vertices.data(), vertexCount, indices.data(), areas.data(), triangleCount, *work.solid, config.walkableClimb))
		{
			p_error = "the geometry could not be rasterized";
			return false;
		}

		rcFilterLowHangingWalkableObstacles(&context, config.walkableClimb, *work.solid);
		rcFilterLedgeSpans(&context, config.walkableHeight, config.walkableClimb, *work.solid);
		rcFilterWalkableLowHeightSpans(&context, config.walkableHeight, *work.solid);

		work.compact = rcAllocCompactHeightfield();
		if (!work.compact || !rcBuildCompactHeightfield(&context, config.walkableHeight, config.walkableClimb, *work.solid, *work.compact))
		{
			p_error = "out of memory for a tile's compact height field";
			return false;
		}

		if (!rcErodeWalkableArea(&context, config.walkableRadius, *work.compact) ||
			!rcBuildDistanceField(&context, *work.compact) ||
			!rcBuildRegions(&context, *work.compact, config.borderSize, config.minRegionArea, config.mergeRegionArea))
		{
			p_error = "a tile's regions could not be built";
			return false;
		}

		work.contours = rcAllocContourSet();
		if (!work.contours || !rcBuildContours(&context, *work.compact, config.maxSimplificationError, config.maxEdgeLen, *work.contours))
		{
			p_error = "a tile's contours could not be built";
			return false;
		}

		if (work.contours->nconts == 0)
		{
			return false;
		}

		work.polygons = rcAllocPolyMesh();
		work.detail = rcAllocPolyMeshDetail();
		if (!work.polygons || !work.detail ||
			!rcBuildPolyMesh(&context, *work.contours, config.maxVertsPerPoly, *work.polygons) ||
			!rcBuildPolyMeshDetail(&context, *work.polygons, *work.compact, config.detailSampleDist, config.detailSampleMaxError, *work.detail))
		{
			p_error = "a tile's polygons could not be built";
			return false;
		}

		if (work.polygons->npolys == 0)
		{
			return false;
		}

		for (int polygon = 0; polygon < work.polygons->npolys; ++polygon)
		{
			work.polygons->flags[polygon] = work.polygons->areas[polygon] == RC_WALKABLE_AREA ? kWalkableFlag : 0;
		}

		dtNavMeshCreateParams parameters{};
		parameters.verts = work.polygons->verts;
		parameters.vertCount = work.polygons->nverts;
		parameters.polys = work.polygons->polys;
		parameters.polyAreas = work.polygons->areas;
		parameters.polyFlags = work.polygons->flags;
		parameters.polyCount = work.polygons->npolys;
		parameters.nvp = work.polygons->nvp;
		parameters.detailMeshes = work.detail->meshes;
		parameters.detailVerts = work.detail->verts;
		parameters.detailVertsCount = work.detail->nverts;
		parameters.detailTris = work.detail->tris;
		parameters.detailTriCount = work.detail->ntris;
		parameters.walkableHeight = s.agentHeight;
		parameters.walkableRadius = s.agentRadius;
		parameters.walkableClimb = s.agentMaxClimb;
		parameters.tileX = p_x;
		parameters.tileY = p_z;
		parameters.tileLayer = 0;
		rcVcopy(parameters.bmin, work.polygons->bmin);
		rcVcopy(parameters.bmax, work.polygons->bmax);
		parameters.cs = config.cs;
		parameters.ch = config.ch;
		parameters.buildBvTree = true;

		if (!dtCreateNavMeshData(&parameters, &p_data, &p_size))
		{
			p_error = std::format("tile ({}, {}) has more vertices than Detour holds: lower tileSize", p_x, p_z);
			return false;
		}

		return true;
	}

	/**
	* Builds and adds the tiles of a range, replacing those there
	*/
	std::optional<std::string> BuildTiles(const NavGeometry& p_geometry, int p_x0, int p_z0, int p_x1, int p_z1)
	{
		const auto bins = BinTriangles(p_geometry, p_x0, p_z0, p_x1, p_z1);
		const int width = p_x1 - p_x0 + 1;

		for (int z = p_z0; z <= p_z1; ++z)
		{
			for (int x = p_x0; x <= p_x1; ++x)
			{
				mesh->removeTile(mesh->getTileRefAt(x, z, 0), nullptr, nullptr);

				unsigned char* data = nullptr;
				int size = 0;
				std::string error;

				if (BuildTile(p_geometry, bins[static_cast<size_t>(z - p_z0) * width + (x - p_x0)], x, z, data, size, error))
				{
					if (dtStatusFailed(mesh->addTile(data, size, DT_TILE_FREE_DATA, 0, nullptr)))
					{
						dtFree(data);
						return std::format("tile ({}, {}) could not be added: too many polygons for the number of tiles", x, z);
					}
				}
				else if (!error.empty())
				{
					return error;
				}
			}
		}

		return std::nullopt;
	}

	/**
	* The nearest polygon to a point, and the point on it
	*/
	dtPolyRef Nearest(const FVector3& p_point, const FVector3& p_extents, float* p_nearest) const
	{
		float center[3], extents[3];
		ToArray(p_point, center);
		ToArray(p_extents, extents);

		dtPolyRef reference = 0;
		if (dtStatusFailed(query->findNearestPoly(center, extents, &filter, &reference, p_nearest)))
		{
			return 0;
		}

		return reference;
	}
};

OvCore::Navigation::NavMesh::NavMesh() : m_impl(std::make_unique<Impl>())
{
	m_impl->filter.setIncludeFlags(kWalkableFlag);
	m_impl->filter.setExcludeFlags(0);
}

OvCore::Navigation::NavMesh::~NavMesh() = default;

OvCore::Navigation::NavMeshSettings OvCore::Navigation::NavMesh::Sanitize(const NavMeshSettings& p_settings)
{
	const auto finite = [](float p_value, float p_fallback) { return std::isfinite(p_value) ? p_value : p_fallback; };
	const NavMeshSettings defaults;

	NavMeshSettings settings = p_settings;
	settings.agentRadius = std::clamp(finite(settings.agentRadius, defaults.agentRadius), 0.0f, 50.0f);
	settings.agentHeight = std::clamp(finite(settings.agentHeight, defaults.agentHeight), 0.1f, 100.0f);
	settings.agentMaxClimb = std::clamp(finite(settings.agentMaxClimb, defaults.agentMaxClimb), 0.0f, 100.0f);
	settings.agentMaxSlope = std::clamp(finite(settings.agentMaxSlope, defaults.agentMaxSlope), 0.0f, 89.0f);
	settings.cellSize = std::clamp(finite(settings.cellSize, defaults.cellSize), 0.01f, 10.0f);
	settings.cellHeight = std::clamp(finite(settings.cellHeight, defaults.cellHeight), 0.01f, 10.0f);
	settings.tileSize = std::clamp<uint32_t>(settings.tileSize, 16, 256);
	return settings;
}

std::optional<std::string> OvCore::Navigation::NavMesh::Build(const NavGeometry& p_geometry, const NavMeshSettings& p_settings)
{
	Clear();
	auto& impl = *m_impl;
	impl.settings = Sanitize(p_settings);

	if (p_geometry.GetTriangleCount() == 0)
	{
		return "no geometry: no static collider in the layers given";
	}

	float low[3] = { FLT_MAX, FLT_MAX, FLT_MAX };
	float high[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
	for (size_t i = 0; i + 2 < p_geometry.vertices.size(); i += 3)
	{
		for (int axis = 0; axis < 3; ++axis)
		{
			low[axis] = std::min(low[axis], p_geometry.vertices[i + axis]);
			high[axis] = std::max(high[axis], p_geometry.vertices[i + axis]);
		}
	}

	const float tile = impl.TileWidth();
	impl.tilesX = std::max(1, static_cast<int>(std::ceil((high[0] - low[0]) / tile)));
	impl.tilesZ = std::max(1, static_cast<int>(std::ceil((high[2] - low[2]) / tile)));
	const uint32_t tiles = static_cast<uint32_t>(impl.tilesX) * static_cast<uint32_t>(impl.tilesZ);

	if (tiles > kMaxTiles)
	{
		const auto count = tiles;
		impl.tilesX = impl.tilesZ = 0;
		return std::format("the geometry spans {} tiles, more than {}: raise tileSize or cellSize", count, kMaxTiles);
	}

	rcVcopy(impl.origin, low);
	// Room above the highest geometry for an agent standing on it
	impl.minY = low[1] - 1.0f;
	impl.maxY = high[1] + impl.settings.agentHeight + 1.0f;

	const int tileBits = std::min(Log2(NextPowerOfTwo(tiles)), 14);
	dtNavMeshParams parameters{};
	rcVcopy(parameters.orig, impl.origin);
	parameters.tileWidth = tile;
	parameters.tileHeight = tile;
	parameters.maxTiles = 1 << tileBits;
	parameters.maxPolys = 1 << (kReferenceBits - tileBits);

	impl.mesh = dtAllocNavMesh();
	if (!impl.mesh || dtStatusFailed(impl.mesh->init(&parameters)))
	{
		impl.Release();
		return "the navigation mesh could not be made";
	}

	if (auto error = impl.BuildTiles(p_geometry, 0, 0, impl.tilesX - 1, impl.tilesZ - 1))
	{
		impl.Release();
		return error;
	}

	impl.query = dtAllocNavMeshQuery();
	if (!impl.query || dtStatusFailed(impl.query->init(impl.mesh, kMaxQueryNodes)))
	{
		impl.Release();
		return "the navigation mesh's queries could not be made";
	}

	return std::nullopt;
}

std::optional<std::string> OvCore::Navigation::NavMesh::RebuildTiles(const NavGeometry& p_geometry, const OvMaths::FVector3& p_min, const OvMaths::FVector3& p_max)
{
	auto& impl = *m_impl;

	if (!IsBuilt())
	{
		return "no navigation mesh is built";
	}

	// The tiles a box touches, borders included: an agent near an edge of the box may be kept from it
	const float tile = impl.TileWidth();
	const float border = static_cast<float>(impl.BorderCells()) * impl.settings.cellSize;
	const int x0 = std::max(0, static_cast<int>(std::floor((std::min(p_min.x, p_max.x) - border - impl.origin[0]) / tile)));
	const int z0 = std::max(0, static_cast<int>(std::floor((std::min(p_min.z, p_max.z) - border - impl.origin[2]) / tile)));
	const int x1 = std::min(impl.tilesX - 1, static_cast<int>(std::floor((std::max(p_min.x, p_max.x) + border - impl.origin[0]) / tile)));
	const int z1 = std::min(impl.tilesZ - 1, static_cast<int>(std::floor((std::max(p_min.z, p_max.z) + border - impl.origin[2]) / tile)));

	if (x0 > x1 || z0 > z1)
	{
		return std::nullopt;
	}

	return impl.BuildTiles(p_geometry, x0, z0, x1, z1);
}

void OvCore::Navigation::NavMesh::Clear()
{
	m_impl->Release();
}

bool OvCore::Navigation::NavMesh::IsBuilt() const
{
	return m_impl->mesh && m_impl->query;
}

const OvCore::Navigation::NavMeshSettings& OvCore::Navigation::NavMesh::GetSettings() const
{
	return m_impl->settings;
}

std::pair<OvMaths::FVector3, OvMaths::FVector3> OvCore::Navigation::NavMesh::GetBounds() const
{
	if (!IsBuilt())
	{
		return { FVector3::Zero, FVector3::Zero };
	}

	const auto& impl = *m_impl;
	const float tile = impl.TileWidth();
	return {
		{ impl.origin[0], impl.minY, impl.origin[2] },
		{ impl.origin[0] + tile * static_cast<float>(impl.tilesX), impl.maxY, impl.origin[2] + tile * static_cast<float>(impl.tilesZ) }
	};
}

uint32_t OvCore::Navigation::NavMesh::GetTileCount() const
{
	uint32_t count = 0;

	if (const dtNavMesh* mesh = m_impl->mesh)
	{
		for (int i = 0; i < mesh->getMaxTiles(); ++i)
		{
			const dtMeshTile* tile = mesh->getTile(i);
			count += tile && tile->header && tile->header->polyCount > 0 ? 1 : 0;
		}
	}

	return count;
}

uint32_t OvCore::Navigation::NavMesh::GetPolygonCount() const
{
	uint32_t count = 0;

	if (const dtNavMesh* mesh = m_impl->mesh)
	{
		for (int i = 0; i < mesh->getMaxTiles(); ++i)
		{
			const dtMeshTile* tile = mesh->getTile(i);
			count += tile && tile->header ? static_cast<uint32_t>(tile->header->polyCount) : 0;
		}
	}

	return count;
}

std::optional<OvCore::Navigation::NavPath> OvCore::Navigation::NavMesh::FindPath(const OvMaths::FVector3& p_from, const OvMaths::FVector3& p_to, const OvMaths::FVector3& p_extents) const
{
	if (!IsBuilt())
	{
		return std::nullopt;
	}

	const auto& impl = *m_impl;
	float start[3], end[3];
	const dtPolyRef startReference = impl.Nearest(p_from, p_extents, start);
	const dtPolyRef endReference = impl.Nearest(p_to, p_extents, end);

	if (!startReference || !endReference)
	{
		return std::nullopt;
	}

	dtPolyRef polygons[kMaxPathPolygons];
	int polygonCount = 0;
	const dtStatus status = impl.query->findPath(startReference, endReference, start, end, &impl.filter, polygons, &polygonCount, kMaxPathPolygons);

	if (dtStatusFailed(status) || polygonCount == 0)
	{
		return std::nullopt;
	}

	NavPath path;
	path.complete = polygons[polygonCount - 1] == endReference && !dtStatusDetail(status, DT_PARTIAL_RESULT) && !dtStatusDetail(status, DT_OUT_OF_NODES);

	// A path that stops short ends at the point of its last polygon nearest to the end
	float goal[3];
	dtVcopy(goal, end);
	if (polygons[polygonCount - 1] != endReference)
	{
		impl.query->closestPointOnPoly(polygons[polygonCount - 1], end, goal, nullptr);
	}

	float corners[kMaxPathCorners * 3];
	unsigned char flags[kMaxPathCorners];
	dtPolyRef references[kMaxPathCorners];
	int cornerCount = 0;

	if (dtStatusFailed(impl.query->findStraightPath(start, goal, polygons, polygonCount, corners, flags, references, &cornerCount, kMaxPathCorners)))
	{
		return std::nullopt;
	}

	path.corners.reserve(cornerCount);
	for (int i = 0; i < cornerCount; ++i)
	{
		path.corners.push_back(ToVector(corners + i * 3));
	}

	return path;
}

std::optional<OvMaths::FVector3> OvCore::Navigation::NavMesh::FindNearestPoint(const OvMaths::FVector3& p_point, const OvMaths::FVector3& p_extents) const
{
	if (!IsBuilt())
	{
		return std::nullopt;
	}

	float nearest[3];
	if (!m_impl->Nearest(p_point, p_extents, nearest))
	{
		return std::nullopt;
	}

	return ToVector(nearest);
}

std::optional<OvCore::Navigation::NavRaycastHit> OvCore::Navigation::NavMesh::Raycast(const OvMaths::FVector3& p_from, const OvMaths::FVector3& p_to) const
{
	if (!IsBuilt())
	{
		return std::nullopt;
	}

	const auto& impl = *m_impl;
	float start[3], end[3];
	const dtPolyRef startReference = impl.Nearest(p_from, kDefaultExtents, start);

	if (!startReference)
	{
		return std::nullopt;
	}

	ToArray(p_to, end);
	end[1] = start[1];

	float fraction = 0.0f;
	float normal[3] = { 0.0f, 0.0f, 0.0f };
	dtPolyRef polygons[kMaxPathPolygons];
	int polygonCount = 0;

	if (dtStatusFailed(impl.query->raycast(startReference, start, end, &impl.filter, &fraction, normal, polygons, &polygonCount, kMaxPathPolygons)) || polygonCount == 0)
	{
		return std::nullopt;
	}

	NavRaycastHit hit;
	hit.hit = fraction <= 1.0f;
	hit.fraction = hit.hit ? fraction : 1.0f;
	hit.normal = ToVector(normal);

	// The point along the way, at the height of the last polygon walked on
	float point[3];
	dtVlerp(point, start, end, hit.fraction);
	float onPolygon[3];
	if (dtStatusSucceed(impl.query->closestPointOnPoly(polygons[polygonCount - 1], point, onPolygon, nullptr)))
	{
		point[1] = onPolygon[1];
	}

	hit.point = ToVector(point);
	return hit;
}

std::optional<OvMaths::FVector3> OvCore::Navigation::NavMesh::GetRandomPoint()
{
	if (!IsBuilt())
	{
		return std::nullopt;
	}

	auto& impl = *m_impl;
	t_randomState = &impl.random;

	dtPolyRef reference = 0;
	float point[3];
	if (dtStatusFailed(impl.query->findRandomPoint(&impl.filter, NextRandom, &reference, point)) || !reference)
	{
		return std::nullopt;
	}

	return ToVector(point);
}

std::optional<OvMaths::FVector3> OvCore::Navigation::NavMesh::GetRandomPointAround(const OvMaths::FVector3& p_center, float p_radius)
{
	if (!IsBuilt() || !(p_radius >= 0.0f))
	{
		return std::nullopt;
	}

	auto& impl = *m_impl;
	float center[3];
	const dtPolyRef startReference = impl.Nearest(p_center, kDefaultExtents, center);

	if (!startReference)
	{
		return std::nullopt;
	}

	t_randomState = &impl.random;

	// Detour draws among the polygons the circle reaches, which may stick out of it: a point out of it is drawn again
	for (int attempt = 0; attempt < kRandomAttempts; ++attempt)
	{
		dtPolyRef reference = 0;
		float point[3];
		if (dtStatusFailed(impl.query->findRandomPointAroundCircle(startReference, center, p_radius, &impl.filter, NextRandom, &reference, point)) || !reference)
		{
			return std::nullopt;
		}

		const float dx = point[0] - center[0];
		const float dz = point[2] - center[2];
		if (dx * dx + dz * dz <= p_radius * p_radius)
		{
			return ToVector(point);
		}
	}

	// A circle smaller than any polygon around it: its center, the nearest point of the mesh
	return ToVector(center);
}

void OvCore::Navigation::NavMesh::SetSeed(uint64_t p_seed)
{
	// splitmix64, so that seeds near each other draw far apart
	uint64_t seed = p_seed + 0x9E3779B97F4A7C15ull;
	seed = (seed ^ (seed >> 30)) * 0xBF58476D1CE4E5B9ull;
	seed = (seed ^ (seed >> 27)) * 0x94D049BB133111EBull;
	seed ^= seed >> 31;
	m_impl->random = seed != 0 ? seed : 1;
}

OvCore::Navigation::NavGeometry OvCore::Navigation::NavMesh::GetDebugGeometry() const
{
	NavGeometry geometry;
	const dtNavMesh* mesh = m_impl->mesh;

	if (!mesh)
	{
		return geometry;
	}

	for (int i = 0; i < mesh->getMaxTiles(); ++i)
	{
		const dtMeshTile* tile = mesh->getTile(i);
		if (!tile || !tile->header)
		{
			continue;
		}

		for (int p = 0; p < tile->header->polyCount; ++p)
		{
			const dtPoly& polygon = tile->polys[p];
			if (polygon.getType() != DT_POLYTYPE_GROUND)
			{
				continue;
			}

			// The detail triangles follow the ground's heights, where the polygon alone is flat between its corners
			const dtPolyDetail& detail = tile->detailMeshes[p];
			for (int t = 0; t < detail.triCount; ++t)
			{
				const unsigned char* triangle = &tile->detailTris[(detail.triBase + t) * 4];
				FVector3 corners[3];

				for (int k = 0; k < 3; ++k)
				{
					const float* vertex = triangle[k] < polygon.vertCount ?
						&tile->verts[polygon.verts[triangle[k]] * 3] :
						&tile->detailVerts[(detail.vertBase + triangle[k] - polygon.vertCount) * 3];
					corners[k] = ToVector(vertex);
				}

				geometry.AddTriangle(corners[0], corners[1], corners[2]);
			}
		}
	}

	return geometry;
}
