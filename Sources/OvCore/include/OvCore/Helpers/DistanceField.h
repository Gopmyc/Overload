/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include <OvMaths/FVector3.h>
#include <OvRendering/Geometry/Vertex.h>

namespace OvCore::Helpers
{
	/**
	* Vertices and triangles made on the CPU, for a model (Resources.CreateModel) or a physical mesh, without going
	* through script tables
	*/
	struct MeshData
	{
		std::vector<OvRendering::Geometry::Vertex> vertices;
		std::vector<uint32_t> indices;
	};

	/**
	* A signed distance field of empty spaces carved in solid ground: negative where it is empty, positive in the
	* solid, about the distance to the surface between them. The empty spaces are tubes (a segment with a radius at each
	* end, squashed and cut flat at the bottom so that a body walks on it) and ellipsoids (cut flat too), blended
	* together over the blend radius and roughened by a fractal value noise, its full depth on the walls and ceilings
	* but only the floor share of it on the floors (within a metre and a half above the nearest space's floor), so
	* that they stay walkable; pillars stay solid inside them, roughened too. A relief can add ledges (strata on the
	* walls, a step every ledge height) and ridges (the noise folded into sharp crests) to them, off the floors as well.
	* Boxes are empty spaces of another kind, built rooms: sharp and exact, neither blended nor roughened, their floor
	* flat or rising along X or Z (a ramp, for stairs), joined to the rest as they are.
	*
	* With a ground (a square grid of heights split as a physical height field is), the empty spaces only count under a
	* cover of solid below it, but at its openings: discs where the cover lifts and the air above the ground counts as
	* empty too, so that the surface meets the ground there. Lights make a vertex's openness: 1 at a light, falling to 0
	* at its radius.
	*
	* Mesh makes the surface between the empty and the solid of a block of voxels (surface nets): two neighbouring
	* blocks meet without a crack, the triangles face the empty side, and over a ground none stands above it.
	*/
	class DistanceField
	{
	public:
		/**
		* A point of the surface, and its normal (towards the empty side), to put things on
		*/
		struct Sample
		{
			OvMaths::FVector3 position;
			OvMaths::FVector3 normal;
		};

		/**
		* Defines the seed of the noise
		* @param p_seed
		*/
		void SetSeed(uint32_t p_seed);

		/**
		* Defines the noise roughening the surfaces: metres at most, how many waves a metre, octaves (1 to 6)
		* @param p_amplitude
		* @param p_frequency
		* @param p_octaves
		*/
		void SetNoise(float p_amplitude, float p_frequency, uint32_t p_octaves, float p_floorShare = 0.2f);

		/**
		* Defines over how many metres two empty spaces blend into one
		* @param p_radius
		*/
		void SetBlend(float p_radius);

		/**
		* Defines the relief of the empty spaces' walls (not the boxes'): a ledge every p_ledgeHeight metres, its step
		* p_ledgeDepth metres deep (0 for none), and p_ridgeShare (0 to 1) of the noise folded into ridges
		*/
		void SetRelief(float p_ledgeHeight, float p_ledgeDepth, float p_ridgeShare);

		/**
		* Adds an empty tube from p_from to p_to, p_fromRadius to p_toRadius wide; its height is p_squash times its
		* width, and its floor is cut flat p_floor of its half height below its axis (1 for no cut)
		*/
		void AddTube(const OvMaths::FVector3& p_from, const OvMaths::FVector3& p_to, float p_fromRadius, float p_toRadius, float p_floor, float p_squash);

		/**
		* Adds an empty ellipsoid, its floor cut flat p_floor of its vertical radius below its centre (1 for no cut)
		*/
		void AddEllipsoid(const OvMaths::FVector3& p_centre, const OvMaths::FVector3& p_radii, float p_floor);

		/**
		* Adds a solid vertical pillar, p_height tall from p_base, kept solid within the empty spaces
		*/
		void AddPillar(const OvMaths::FVector3& p_base, float p_height, float p_radius);

		/**
		* Adds an empty box from p_min to p_max, sharp and unroughened; its floor rises by p_rise from the low to the
		* high side along p_axis (0 for X, 2 for Z; 0 rise for a flat floor), its ceiling stays at p_max's height
		*/
		void AddBox(const OvMaths::FVector3& p_min, const OvMaths::FVector3& p_max, float p_rise = 0.0f, uint32_t p_axis = 0);

		/**
		* Defines the ground: p_count * p_count heights, row-major (+X then +Z), p_spacing apart from (p_originX,
		* p_originZ); the empty spaces only count p_cover metres below it, and the air above an opening counts p_sink
		* metres below it. Returns false, and changes nothing, when the heights are not p_count * p_count
		*/
		bool SetGround(uint32_t p_count, float p_spacing, float p_originX, float p_originZ, std::vector<float> p_heights, float p_cover, float p_sink);

		/**
		* Adds an opening: a disc of the ground where the cover lifts (fully within 70 % of its radius)
		*/
		void AddOpening(float p_x, float p_z, float p_radius);

		/**
		* Adds a light for the vertices' openness
		*/
		void AddLight(const OvMaths::FVector3& p_position, float p_radius);

		/**
		* Returns the value of the field at a point: negative where it is empty
		* @param p_point
		*/
		float GetValue(const OvMaths::FVector3& p_point) const;

		/**
		* Returns the height of the floor under an empty point, within p_depth metres, or nothing (the point is solid,
		* or there is no floor so near)
		* @param p_point
		* @param p_depth
		*/
		std::optional<float> FindFloor(const OvMaths::FVector3& p_point, float p_depth) const;

		/**
		* Returns the height of the ground at (x, z), or nothing without a ground
		*/
		std::optional<float> GetGroundHeight(float p_x, float p_z) const;

		/**
		* Makes the surface of the block of p_cells voxels a side, p_voxel metres each, whose first voxel is at
		* (p_x, p_y, p_z) * p_voxel; nothing if no surface crosses it. A vertex's texture coordinates are its depth
		* under the ground (0 without a ground) and its openness. Fills p_samples, if given, with about one point of the
		* surface every p_sampleSpacing metres
		*/
		std::optional<MeshData> Mesh(int32_t p_x, int32_t p_y, int32_t p_z, uint32_t p_cells, float p_voxel, std::vector<Sample>* p_samples = nullptr, float p_sampleSpacing = 2.0f) const;

	private:
		struct Primitive
		{
			enum class EKind : uint8_t { TUBE, ELLIPSOID, PILLAR, BOX };

			EKind kind;
			OvMaths::FVector3 a;
			OvMaths::FVector3 b;
			float r0 = 0.0f;
			float r1 = 0.0f;
			float floor = 1.0f;
			float squash = 1.0f;
			uint32_t axis = 0;
			OvMaths::FVector3 boundsMin;
			OvMaths::FVector3 boundsMax;
		};

		struct Opening
		{
			float x;
			float z;
			float radius;
		};

		struct Light
		{
			OvMaths::FVector3 position;
			float radius;
		};

		void AddPrimitive(Primitive p_primitive);
		void BuildGrid() const;
		void Gather(const OvMaths::FVector3& p_min, const OvMaths::FVector3& p_max, std::vector<uint32_t>& p_out) const;
		float Evaluate(const OvMaths::FVector3& p_point, const std::vector<uint32_t>& p_candidates) const;
		float GetDistance(const Primitive& p_primitive, const OvMaths::FVector3& p_point, float* p_floor = nullptr) const;
		float GetNoise(const OvMaths::FVector3& p_point) const;
		float GetRelief(const OvMaths::FVector3& p_point, float p_noise) const;
		float GetOpening(float p_x, float p_z, bool& p_inside) const;
		float GetOpenness(const OvMaths::FVector3& p_point) const;
		float GetGround(float p_x, float p_z) const;

	private:
		uint32_t m_seed = 0;
		float m_noiseAmplitude = 0.0f;
		float m_noiseFrequency = 0.1f;
		uint32_t m_noiseOctaves = 1;
		float m_floorShare = 0.2f;
		float m_blend = 0.0f;
		float m_ledgeHeight = 2.5f;
		float m_ledgeDepth = 0.0f;
		float m_ridgeShare = 0.0f;

		std::vector<Primitive> m_primitives;
		std::vector<Opening> m_openings;
		std::vector<Light> m_lights;

		uint32_t m_groundCount = 0;
		float m_groundSpacing = 1.0f;
		float m_groundOriginX = 0.0f;
		float m_groundOriginZ = 0.0f;
		float m_cover = 0.0f;
		float m_sink = 0.0f;
		std::vector<float> m_ground;

		mutable bool m_gridDirty = true;
		mutable std::unordered_map<uint64_t, std::vector<uint32_t>> m_grid;
	};
}
