/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <OvMaths/FVector3.h>

namespace OvCore::Navigation
{
	/**
	* The agent a navigation mesh is made for, and how finely it is made
	*/
	struct NavMeshSettings
	{
		float agentRadius = 0.4f;		// Keeps a path this far from walls and edges
		float agentHeight = 1.8f;		// Leaves out what is under a ceiling lower than this
		float agentMaxClimb = 0.4f;		// The highest step walked up, or down
		float agentMaxSlope = 45.0f;	// The steepest ground walked on, in degrees
		float cellSize = 0.2f;			// The size of the grid the geometry is sampled on, across
		float cellHeight = 0.1f;		// And up
		uint32_t tileSize = 48;			// Cells across a tile: what Rebuild remakes at once
		uint32_t layerMask = 0xFFFFu;	// The collision layers whose colliders make the geometry (see SceneGeometry)
	};

	/**
	* Triangles in the world, three indices each, counter-clockwise seen from their outside
	*/
	struct NavGeometry
	{
		std::vector<float> vertices;	// x, y, z
		std::vector<int> indices;

		/**
		* Adds a triangle
		* @param p_a
		* @param p_b
		* @param p_c
		*/
		void AddTriangle(const OvMaths::FVector3& p_a, const OvMaths::FVector3& p_b, const OvMaths::FVector3& p_c);

		/**
		* Returns the number of triangles
		*/
		size_t GetTriangleCount() const;
	};

	/**
	* A path's corners, from the start to the end, and whether it reaches the end or stops at the point nearest to it
	*/
	struct NavPath
	{
		std::vector<OvMaths::FVector3> corners;
		bool complete = false;
	};

	/**
	* Where a walk along the navigation mesh stops
	*/
	struct NavRaycastHit
	{
		bool hit = false;				// True when an edge stopped it before the end
		OvMaths::FVector3 point;		// Where it stopped, on the mesh
		OvMaths::FVector3 normal;		// The edge's, across, when it hit one
		float fraction = 1.0f;			// The share of the way walked
	};

	/**
	* A navigation mesh built from triangles with Recast, in tiles, and queried with Detour
	*/
	class NavMesh
	{
	public:
		/**
		* The size of the box around a point in which the mesh is looked for, by default
		*/
		static const OvMaths::FVector3 kDefaultExtents;

		/**
		* The most tiles a mesh may have
		*/
		static constexpr uint32_t kMaxTiles = 1u << 14;

		NavMesh();
		~NavMesh();

		NavMesh(const NavMesh&) = delete;
		NavMesh& operator=(const NavMesh&) = delete;

		/**
		* Brings each setting into its range
		* @param p_settings
		*/
		static NavMeshSettings Sanitize(const NavMeshSettings& p_settings);

		/**
		* Builds the mesh over the geometry, replacing any built before. Returns nothing once built, else why not
		* @param p_geometry
		* @param p_settings
		*/
		std::optional<std::string> Build(const NavGeometry& p_geometry, const NavMeshSettings& p_settings);

		/**
		* Builds the tiles that a box touches again, from the geometry given (the geometry of the whole scene, or of
		* the box and around it). Returns nothing once rebuilt, else why not
		* @param p_geometry
		* @param p_min
		* @param p_max
		*/
		std::optional<std::string> RebuildTiles(const NavGeometry& p_geometry, const OvMaths::FVector3& p_min, const OvMaths::FVector3& p_max);

		/**
		* Removes the mesh
		*/
		void Clear();

		/**
		* Returns true once a mesh is built
		*/
		bool IsBuilt() const;

		/**
		* Returns the settings of the mesh built, or of the next
		*/
		const NavMeshSettings& GetSettings() const;

		/**
		* Returns the box of the mesh, or of nothing when none is built
		*/
		std::pair<OvMaths::FVector3, OvMaths::FVector3> GetBounds() const;

		/**
		* Returns the tiles that hold polygons
		*/
		uint32_t GetTileCount() const;

		/**
		* Returns the polygons
		*/
		uint32_t GetPolygonCount() const;

		/**
		* Returns the path between two points, each taken to the nearest point of the mesh, or nothing when either is
		* too far from it
		* @param p_from
		* @param p_to
		* @param p_extents (around each point, where the mesh is looked for)
		*/
		std::optional<NavPath> FindPath(const OvMaths::FVector3& p_from, const OvMaths::FVector3& p_to, const OvMaths::FVector3& p_extents = kDefaultExtents) const;

		/**
		* Returns the nearest point of the mesh, or nothing when the mesh is not within the extents
		* @param p_point
		* @param p_extents
		*/
		std::optional<OvMaths::FVector3> FindNearestPoint(const OvMaths::FVector3& p_point, const OvMaths::FVector3& p_extents = kDefaultExtents) const;

		/**
		* Walks along the mesh from a point towards another, stopping at the first edge, or nothing when the start is
		* not near the mesh
		* @param p_from
		* @param p_to
		*/
		std::optional<NavRaycastHit> Raycast(const OvMaths::FVector3& p_from, const OvMaths::FVector3& p_to) const;

		/**
		* Returns a point of the mesh drawn at random, the larger polygons the likelier
		*/
		std::optional<OvMaths::FVector3> GetRandomPoint();

		/**
		* Returns a point of the mesh drawn at random within a distance of a point, and reachable from it
		* @param p_center
		* @param p_radius
		*/
		std::optional<OvMaths::FVector3> GetRandomPointAround(const OvMaths::FVector3& p_center, float p_radius);

		/**
		* Sets the seed of the random points: the same seed gives the same points
		* @param p_seed
		*/
		void SetSeed(uint64_t p_seed);

		/**
		* Returns the mesh's surface as triangles, for drawing it
		*/
		NavGeometry GetDebugGeometry() const;

	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
