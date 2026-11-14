/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cmath>
#include <numbers>

#include <OvCore/Terrain/Heightmap.h>
#include <OvCore/Terrain/Scatter.h>

namespace
{
	// The random numbers of one cell, from the seed and the cell's coordinates (splitmix64): the same on every platform
	class CellRandom
	{
	public:
		CellRandom(uint32_t p_seed, uint32_t p_cellX, uint32_t p_cellZ) :
			m_state(
				static_cast<uint64_t>(p_seed) * 0x9E3779B97F4A7C15ull ^
				static_cast<uint64_t>(p_cellX) * 0xC2B2AE3D27D4EB4Full ^
				static_cast<uint64_t>(p_cellZ) * 0x165667B19E3779F9ull
			)
		{
		}

		// From 0 (included) to 1 (excluded)
		float Next()
		{
			m_state += 0x9E3779B97F4A7C15ull;
			uint64_t z = m_state;
			z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
			z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
			z ^= z >> 31;
			return static_cast<float>(z >> 40) * (1.0f / 16777216.0f);
		}

	private:
		uint64_t m_state;
	};

	// The first cell whose corner lies at or after a distance from the field's corner
	uint32_t FirstCell(float p_distance, float p_cellSize)
	{
		return static_cast<uint32_t>(std::max(std::ceil(p_distance / p_cellSize), 0.0f));
	}
}

float OvCore::Terrain::SampleDensity(const std::vector<float>& p_map, uint32_t p_mapSize, float p_u, float p_v)
{
	if (p_mapSize == 0 || p_map.size() != static_cast<size_t>(p_mapSize) * p_mapSize)
	{
		return 1.0f;
	}

	if (p_mapSize == 1)
	{
		return std::clamp(p_map[0], 0.0f, 1.0f);
	}

	const float last = static_cast<float>(p_mapSize - 1);
	const float x = std::clamp(p_u, 0.0f, 1.0f) * last;
	const float z = std::clamp(p_v, 0.0f, 1.0f) * last;
	const uint32_t x0 = std::min(static_cast<uint32_t>(x), p_mapSize - 2);
	const uint32_t z0 = std::min(static_cast<uint32_t>(z), p_mapSize - 2);
	const float fx = x - static_cast<float>(x0);
	const float fz = z - static_cast<float>(z0);

	const auto at = [&](uint32_t p_x, uint32_t p_z) { return p_map[static_cast<size_t>(p_z) * p_mapSize + p_x]; };
	const float near = at(x0, z0) + fx * (at(x0 + 1, z0) - at(x0, z0));
	const float far = at(x0, z0 + 1) + fx * (at(x0 + 1, z0 + 1) - at(x0, z0 + 1));
	return std::clamp(near + fz * (far - near), 0.0f, 1.0f);
}

void OvCore::Terrain::ScatterChunk(
	const std::vector<float>& p_heights,
	const TerrainLayout& p_layout,
	uint32_t p_chunk,
	const ScatterSettings& p_settings,
	const std::vector<float>& p_densityMap,
	uint32_t p_densityMapSize,
	std::vector<ScatterInstance>& p_instances
)
{
	p_instances.clear();

	const float density = std::min(p_settings.density, kMaxScatterDensity);
	if (!(density > 0.0f) || p_chunk >= p_layout.GetChunkCount() || p_layout.spacing <= 0.0f)
	{
		return;
	}

	uint32_t x0 = 0, x1 = 0, z0 = 0, z1 = 0;
	p_layout.GetChunkSamples(p_chunk, x0, x1, z0, z1);

	const float spacing = p_layout.spacing;
	const float extent = static_cast<float>(p_layout.sampleCount - 1) * spacing;
	const float cellSize = 1.0f / std::sqrt(density);

	// The cells whose corner lies in this chunk: the same bound on both sides of a shared edge
	const uint32_t cellX0 = FirstCell(static_cast<float>(x0) * spacing, cellSize);
	const uint32_t cellX1 = FirstCell(static_cast<float>(x1) * spacing, cellSize);
	const uint32_t cellZ0 = FirstCell(static_cast<float>(z0) * spacing, cellSize);
	const uint32_t cellZ1 = FirstCell(static_cast<float>(z1) * spacing, cellSize);

	const float minScale = std::min(p_settings.minScale, p_settings.maxScale);
	const float maxScale = std::max(p_settings.minScale, p_settings.maxScale);
	const float alignment = std::clamp(p_settings.slopeAlignment, 0.0f, 1.0f);
	const float maxSlope = std::clamp(p_settings.maxSlope, 0.0f, 90.0f) * std::numbers::pi_v<float> / 180.0f;

	for (uint32_t cellZ = cellZ0; cellZ < cellZ1; ++cellZ)
	{
		for (uint32_t cellX = cellX0; cellX < cellX1; ++cellX)
		{
			// Every number is drawn, kept or not, so a candidate does not depend on the ones refused before it
			CellRandom random(p_settings.seed, cellX, cellZ);
			const float x = (static_cast<float>(cellX) + random.Next()) * cellSize;
			const float z = (static_cast<float>(cellZ) + random.Next()) * cellSize;
			const float keep = random.Next();
			const float yaw = random.Next() * 2.0f * std::numbers::pi_v<float>;
			const float size = random.Next();

			if (x > extent || z > extent)
			{
				continue;
			}

			if (keep >= SampleDensity(p_densityMap, p_densityMapSize, x / extent, z / extent))
			{
				continue;
			}

			const auto sample = SampleHeightField(p_heights, p_layout.sampleCount, spacing, x, z);
			if (!sample || sample->height < p_settings.minHeight || sample->height > p_settings.maxHeight)
			{
				continue;
			}

			const auto normal = OvMaths::FVector3::Normalize({ -sample->slopeX, 1.0f, -sample->slopeZ });
			const float slope = std::acos(std::clamp(normal.y, -1.0f, 1.0f));
			if (slope > maxSlope + 1e-5f)
			{
				continue;
			}

			ScatterInstance instance;
			instance.position = { x, sample->height, z };
			instance.scale = minScale + size * (maxScale - minScale);

			// A turn about the vertical, then a tilt towards the ground's normal
			const float halfYaw = p_settings.randomYaw ? yaw * 0.5f : 0.0f;
			const OvMaths::FQuaternion turn{ 0.0f, std::sin(halfYaw), 0.0f, std::cos(halfYaw) };

			OvMaths::FQuaternion tilt;
			const float axisLength = std::sqrt(normal.z * normal.z + normal.x * normal.x);
			if (alignment > 0.0f && axisLength > 1e-6f)
			{
				const float halfTilt = slope * alignment * 0.5f;
				const float s = std::sin(halfTilt) / axisLength;
				tilt = OvMaths::FQuaternion{ normal.z * s, 0.0f, -normal.x * s, std::cos(halfTilt) };
			}

			instance.rotation = tilt * turn;
			p_instances.push_back(instance);
		}
	}
}

OvRendering::Geometry::BoundingSphere OvCore::Terrain::BoundInstances(const std::vector<ScatterInstance>& p_instances, const OvRendering::Geometry::BoundingSphere& p_modelBounds)
{
	if (p_instances.empty())
	{
		return { {}, 0.0f };
	}

	OvMaths::FVector3 low = p_instances.front().position;
	OvMaths::FVector3 high = low;
	float largest = 0.0f;

	for (const auto& instance : p_instances)
	{
		low = { std::min(low.x, instance.position.x), std::min(low.y, instance.position.y), std::min(low.z, instance.position.z) };
		high = { std::max(high.x, instance.position.x), std::max(high.y, instance.position.y), std::max(high.z, instance.position.z) };
		largest = std::max(largest, instance.scale);
	}

	// Each model reaches at most its bounds' offset and radius, scaled, from where it stands, whichever way it turns
	const auto centre = (low + high) * 0.5f;
	const float reach = (OvMaths::FVector3::Length(p_modelBounds.position) + p_modelBounds.radius) * largest;
	return { centre, OvMaths::FVector3::Distance(centre, high) + reach };
}
