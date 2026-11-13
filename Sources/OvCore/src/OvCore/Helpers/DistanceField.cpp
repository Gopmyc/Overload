/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_set>
#include <utility>

#include <OvCore/Helpers/DistanceField.h>

using OvMaths::FVector3;

namespace
{
	// The side of a cell of the grid of primitives, in metres
	constexpr float kGridCell = 8.0f;

	// The value deep in the solid, far from any empty space
	constexpr float kSolid = 1.0e6f;

	// How far above the ground a triangle of the surface may stand (the air over an opening is not drawn)
	constexpr float kAboveGround = 0.02f;

	// How deep under a box's floor the solid stays, whatever empty space comes near
	constexpr float kKeptUnder = 3.0f;

	uint32_t Hash(int32_t p_x, int32_t p_y, int32_t p_z, uint32_t p_seed)
	{
		uint32_t h = p_seed ^ 0x9E3779B9u;
		h ^= static_cast<uint32_t>(p_x) * 0x85EBCA6Bu;
		h = (h ^ (h >> 13)) * 0xC2B2AE35u;
		h ^= static_cast<uint32_t>(p_y) * 0x27D4EB2Fu;
		h = (h ^ (h >> 15)) * 0x165667B1u;
		h ^= static_cast<uint32_t>(p_z) * 0x9E3779B1u;
		h = (h ^ (h >> 16)) * 0x85EBCA77u;
		return h ^ (h >> 13);
	}

	float Unit(uint32_t p_hash)
	{
		return static_cast<float>(p_hash >> 8) * (1.0f / 16777216.0f);
	}

	float SmoothStep(float p_t)
	{
		return p_t * p_t * (3.0f - 2.0f * p_t);
	}

	float Mix(float p_a, float p_b, float p_t)
	{
		return p_a + (p_b - p_a) * p_t;
	}

	// Value noise, 0 to 1, smooth between the corners of a lattice of unit cells
	float ValueNoise(float p_x, float p_y, float p_z, uint32_t p_seed)
	{
		const float fx = std::floor(p_x), fy = std::floor(p_y), fz = std::floor(p_z);
		const int32_t x = static_cast<int32_t>(fx), y = static_cast<int32_t>(fy), z = static_cast<int32_t>(fz);
		const float tx = SmoothStep(p_x - fx), ty = SmoothStep(p_y - fy), tz = SmoothStep(p_z - fz);

		const auto corner = [p_seed](int32_t i, int32_t j, int32_t k) { return Unit(Hash(i, j, k, p_seed)); };

		const float x00 = Mix(corner(x, y, z), corner(x + 1, y, z), tx);
		const float x10 = Mix(corner(x, y + 1, z), corner(x + 1, y + 1, z), tx);
		const float x01 = Mix(corner(x, y, z + 1), corner(x + 1, y, z + 1), tx);
		const float x11 = Mix(corner(x, y + 1, z + 1), corner(x + 1, y + 1, z + 1), tx);

		return Mix(Mix(x00, x10, ty), Mix(x01, x11, ty), tz);
	}

	// The union of two empty spaces, rounded over p_radius where they meet
	float SmoothMin(float p_a, float p_b, float p_radius)
	{
		if (p_radius <= 0.0f)
		{
			return std::min(p_a, p_b);
		}

		const float h = std::max(p_radius - std::abs(p_a - p_b), 0.0f) / p_radius;
		return std::min(p_a, p_b) - h * h * p_radius * 0.25f;
	}

	int32_t CellOf(float p_value)
	{
		return static_cast<int32_t>(std::floor(p_value / kGridCell));
	}

	uint64_t CellKey(int32_t p_x, int32_t p_y, int32_t p_z)
	{
		return (static_cast<uint64_t>(static_cast<uint32_t>(p_x) & 0x1FFFFFu) << 42)
			| (static_cast<uint64_t>(static_cast<uint32_t>(p_y) & 0x1FFFFFu) << 21)
			| static_cast<uint64_t>(static_cast<uint32_t>(p_z) & 0x1FFFFFu);
	}

	bool Inside(const FVector3& p_point, const FVector3& p_min, const FVector3& p_max)
	{
		return p_point.x >= p_min.x && p_point.x <= p_max.x && p_point.y >= p_min.y && p_point.y <= p_max.y && p_point.z >= p_min.z && p_point.z <= p_max.z;
	}

	FVector3 Min(const FVector3& p_a, const FVector3& p_b)
	{
		return { std::min(p_a.x, p_b.x), std::min(p_a.y, p_b.y), std::min(p_a.z, p_b.z) };
	}

	FVector3 Max(const FVector3& p_a, const FVector3& p_b)
	{
		return { std::max(p_a.x, p_b.x), std::max(p_a.y, p_b.y), std::max(p_a.z, p_b.z) };
	}
}

void OvCore::Helpers::DistanceField::SetSeed(uint32_t p_seed)
{
	m_seed = p_seed;
}

void OvCore::Helpers::DistanceField::SetNoise(float p_amplitude, float p_frequency, uint32_t p_octaves, float p_floorShare)
{
	m_noiseAmplitude = std::max(p_amplitude, 0.0f);
	m_noiseFrequency = std::max(p_frequency, 0.0f);
	m_noiseOctaves = std::clamp(p_octaves, 1u, 6u);
	m_floorShare = std::clamp(p_floorShare, 0.0f, 1.0f);
	m_gridDirty = true;
}

void OvCore::Helpers::DistanceField::SetBlend(float p_radius)
{
	m_blend = std::max(p_radius, 0.0f);
	m_gridDirty = true;
}

void OvCore::Helpers::DistanceField::SetRelief(float p_ledgeHeight, float p_ledgeDepth, float p_ridgeShare)
{
	m_ledgeHeight = std::max(p_ledgeHeight, 0.25f);
	m_ledgeDepth = std::max(p_ledgeDepth, 0.0f);
	m_ridgeShare = std::clamp(p_ridgeShare, 0.0f, 1.0f);
	m_gridDirty = true;
}

void OvCore::Helpers::DistanceField::AddTube(const FVector3& p_from, const FVector3& p_to, float p_fromRadius, float p_toRadius, float p_floor, float p_squash)
{
	Primitive primitive{ Primitive::EKind::TUBE, p_from, p_to };
	primitive.r0 = std::max(p_fromRadius, 0.0f);
	primitive.r1 = std::max(p_toRadius, 0.0f);
	primitive.floor = std::clamp(p_floor, 0.0f, 1.0f);
	primitive.squash = std::clamp(p_squash, 0.1f, 1.0f);

	const float radius = std::max(primitive.r0, primitive.r1);
	primitive.boundsMin = Min(p_from, p_to) - FVector3{ radius, radius, radius };
	primitive.boundsMax = Max(p_from, p_to) + FVector3{ radius, radius, radius };

	AddPrimitive(primitive);
}

void OvCore::Helpers::DistanceField::AddEllipsoid(const FVector3& p_centre, const FVector3& p_radii, float p_floor)
{
	Primitive primitive{ Primitive::EKind::ELLIPSOID, p_centre, { std::max(p_radii.x, 0.01f), std::max(p_radii.y, 0.01f), std::max(p_radii.z, 0.01f) } };
	primitive.floor = std::clamp(p_floor, 0.0f, 1.0f);
	primitive.boundsMin = p_centre - primitive.b;
	primitive.boundsMax = p_centre + primitive.b;

	AddPrimitive(primitive);
}

void OvCore::Helpers::DistanceField::AddPillar(const FVector3& p_base, float p_height, float p_radius)
{
	Primitive primitive{ Primitive::EKind::PILLAR, p_base, p_base + FVector3{ 0.0f, std::max(p_height, 0.0f), 0.0f } };
	primitive.r0 = std::max(p_radius, 0.0f);
	primitive.boundsMin = primitive.a - FVector3{ primitive.r0, primitive.r0, primitive.r0 };
	primitive.boundsMax = primitive.b + FVector3{ primitive.r0, primitive.r0, primitive.r0 };

	AddPrimitive(primitive);
}

void OvCore::Helpers::DistanceField::AddBox(const FVector3& p_min, const FVector3& p_max, float p_rise, uint32_t p_axis)
{
	Primitive primitive{ Primitive::EKind::BOX, Min(p_min, p_max), Max(p_min, p_max) };
	primitive.r0 = p_rise;
	primitive.axis = p_axis == 2 ? 2 : 0;
	primitive.boundsMin = primitive.a + FVector3{ 0.0f, std::min(p_rise, 0.0f), 0.0f };
	primitive.boundsMax = primitive.b + FVector3{ 0.0f, std::max(p_rise, 0.0f), 0.0f };

	AddPrimitive(primitive);
}

bool OvCore::Helpers::DistanceField::SetGround(uint32_t p_count, float p_spacing, float p_originX, float p_originZ, std::vector<float> p_heights, float p_cover, float p_sink)
{
	if (p_count < 2 || p_spacing <= 0.0f || p_heights.size() != static_cast<size_t>(p_count) * p_count)
	{
		return false;
	}

	m_groundCount = p_count;
	m_groundSpacing = p_spacing;
	m_groundOriginX = p_originX;
	m_groundOriginZ = p_originZ;
	m_ground = std::move(p_heights);
	m_cover = std::max(p_cover, 0.0f);
	m_sink = std::max(p_sink, 0.0f);

	return true;
}

void OvCore::Helpers::DistanceField::AddOpening(float p_x, float p_z, float p_radius)
{
	m_openings.push_back({ p_x, p_z, std::max(p_radius, 0.0f) });
}

void OvCore::Helpers::DistanceField::AddLight(const FVector3& p_position, float p_radius)
{
	m_lights.push_back({ p_position, std::max(p_radius, 0.0f) });
}

void OvCore::Helpers::DistanceField::AddPrimitive(Primitive p_primitive)
{
	m_primitives.push_back(p_primitive);
	m_gridDirty = true;
}

void OvCore::Helpers::DistanceField::BuildGrid() const
{
	if (!m_gridDirty)
	{
		return;
	}

	m_grid.clear();

	// A primitive reaches past its own bounds by the blend, the noise and the ledges
	const float reach = m_blend + m_noiseAmplitude + m_ledgeDepth + 0.5f;
	const FVector3 inflate{ reach, reach, reach };

	for (uint32_t index = 0; index < m_primitives.size(); ++index)
	{
		const auto& primitive = m_primitives[index];
		const FVector3 low = primitive.boundsMin - inflate;
		const FVector3 high = primitive.boundsMax + inflate;

		for (int32_t z = CellOf(low.z); z <= CellOf(high.z); ++z)
		{
			for (int32_t y = CellOf(low.y); y <= CellOf(high.y); ++y)
			{
				for (int32_t x = CellOf(low.x); x <= CellOf(high.x); ++x)
				{
					m_grid[CellKey(x, y, z)].push_back(index);
				}
			}
		}
	}

	m_gridDirty = false;
}

void OvCore::Helpers::DistanceField::Gather(const FVector3& p_min, const FVector3& p_max, std::vector<uint32_t>& p_out) const
{
	BuildGrid();

	std::unordered_set<uint32_t> seen;

	for (int32_t z = CellOf(p_min.z); z <= CellOf(p_max.z); ++z)
	{
		for (int32_t y = CellOf(p_min.y); y <= CellOf(p_max.y); ++y)
		{
			for (int32_t x = CellOf(p_min.x); x <= CellOf(p_max.x); ++x)
			{
				const auto found = m_grid.find(CellKey(x, y, z));

				if (found == m_grid.end())
				{
					continue;
				}

				for (const uint32_t index : found->second)
				{
					if (seen.insert(index).second)
					{
						p_out.push_back(index);
					}
				}
			}
		}
	}

	std::sort(p_out.begin(), p_out.end());
}

float OvCore::Helpers::DistanceField::GetDistance(const Primitive& p_primitive, const FVector3& p_point, float* p_floor) const
{
	switch (p_primitive.kind)
	{
	case Primitive::EKind::TUBE:
	{
		const FVector3 axis = p_primitive.b - p_primitive.a;
		const float length = FVector3::Dot(axis, axis);
		const float t = length > 0.0f ? std::clamp(FVector3::Dot(p_point - p_primitive.a, axis) / length, 0.0f, 1.0f) : 0.0f;
		const FVector3 centre = p_primitive.a + axis * t;
		const float radius = Mix(p_primitive.r0, p_primitive.r1, t);
		const float squash = p_primitive.squash;
		const float dx = p_point.x - centre.x, dy = (p_point.y - centre.y) / squash, dz = p_point.z - centre.z;
		float distance = (std::sqrt(dx * dx + dy * dy + dz * dz) - radius) * squash;
		const float floor = centre.y - radius * squash * p_primitive.floor;

		if (p_primitive.floor < 1.0f)
		{
			distance = std::max(distance, floor - p_point.y);
		}

		if (p_floor != nullptr)
		{
			*p_floor = floor;
		}

		return distance;
	}
	case Primitive::EKind::ELLIPSOID:
	{
		const FVector3& radii = p_primitive.b;
		const FVector3 local = p_point - p_primitive.a;
		const float k = std::sqrt((local.x / radii.x) * (local.x / radii.x) + (local.y / radii.y) * (local.y / radii.y) + (local.z / radii.z) * (local.z / radii.z));
		float distance = (k - 1.0f) * std::min({ radii.x, radii.y, radii.z });
		const float floor = p_primitive.a.y - radii.y * p_primitive.floor;

		if (p_primitive.floor < 1.0f)
		{
			distance = std::max(distance, floor - p_point.y);
		}

		if (p_floor != nullptr)
		{
			*p_floor = floor;
		}

		return distance;
	}
	case Primitive::EKind::PILLAR:
	{
		const float y = std::clamp(p_point.y, p_primitive.a.y, p_primitive.b.y);
		const float dx = p_point.x - p_primitive.a.x, dy = p_point.y - y, dz = p_point.z - p_primitive.a.z;
		return std::sqrt(dx * dx + dy * dy + dz * dz) - p_primitive.r0;
	}
	case Primitive::EKind::BOX:
	{
		// The farthest of its faces' planes: exact on the faces, which is all the surface needs; the floor a ramp
		const FVector3& low = p_primitive.a;
		const FVector3& high = p_primitive.b;
		const bool alongZ = p_primitive.axis == 2;
		const float span = alongZ ? high.z - low.z : high.x - low.x;
		const float t = span > 0.0f ? std::clamp(((alongZ ? p_point.z - low.z : p_point.x - low.x)) / span, 0.0f, 1.0f) : 0.0f;
		const float slope = span > 0.0f ? p_primitive.r0 / span : 0.0f;
		const float floor = low.y + p_primitive.r0 * t;
		const float ceiling = high.y + std::max(p_primitive.r0, 0.0f);

		if (p_floor != nullptr)
		{
			*p_floor = floor;
		}

		return std::max({
			low.x - p_point.x, p_point.x - high.x, low.z - p_point.z, p_point.z - high.z, p_point.y - ceiling,
			(floor - p_point.y) / std::sqrt(1.0f + slope * slope)
		});
	}
	}

	return kSolid;
}

float OvCore::Helpers::DistanceField::GetNoise(const FVector3& p_point) const
{
	float sum = 0.0f;
	float weight = 1.0f;
	float total = 0.0f;
	float frequency = m_noiseFrequency;

	for (uint32_t octave = 0; octave < m_noiseOctaves; ++octave)
	{
		sum += weight * ValueNoise(p_point.x * frequency, p_point.y * frequency, p_point.z * frequency, m_seed + octave * 101u);
		total += weight;
		weight *= 0.5f;
		frequency *= 2.0f;
	}

	return (sum / total) * 2.0f - 1.0f;
}

float OvCore::Helpers::DistanceField::GetRelief(const FVector3& p_point, float p_noise) const
{
	if (m_ledgeDepth <= 0.0f)
	{
		return 0.0f;
	}

	// A sawtooth up the walls, its rise a shelf and its drop a step, the noise wobbling it off the level
	const float t = p_point.y / m_ledgeHeight + p_noise * 0.35f;
	const float f = t - std::floor(t);
	const float shelf = f < 0.8f ? f / 0.8f : (1.0f - f) / 0.2f;

	return m_ledgeDepth * (shelf - 0.5f);
}

float OvCore::Helpers::DistanceField::GetGround(float p_x, float p_z) const
{
	const float last = static_cast<float>(m_groundCount - 1);
	const float gx = std::clamp((p_x - m_groundOriginX) / m_groundSpacing, 0.0f, last);
	const float gz = std::clamp((p_z - m_groundOriginZ) / m_groundSpacing, 0.0f, last);
	const uint32_t ix = std::min(static_cast<uint32_t>(gx), m_groundCount - 2);
	const uint32_t iz = std::min(static_cast<uint32_t>(gz), m_groundCount - 2);
	const float fx = gx - static_cast<float>(ix), fz = gz - static_cast<float>(iz);

	const auto sample = [this](uint32_t i, uint32_t j) { return m_ground[static_cast<size_t>(j) * m_groundCount + i]; };
	const float h00 = sample(ix, iz), h10 = sample(ix + 1, iz), h01 = sample(ix, iz + 1), h11 = sample(ix + 1, iz + 1);

	// The cell split along its (x, z)-(x + 1, z + 1) diagonal, as the height field is
	if (fz >= fx)
	{
		return h00 + fz * (h01 - h00) + fx * (h11 - h01);
	}

	return h00 + fx * (h10 - h00) + fz * (h11 - h10);
}

std::optional<float> OvCore::Helpers::DistanceField::GetGroundHeight(float p_x, float p_z) const
{
	if (m_ground.empty())
	{
		return std::nullopt;
	}

	return GetGround(p_x, p_z);
}

float OvCore::Helpers::DistanceField::GetOpening(float p_x, float p_z, bool& p_inside) const
{
	float weight = 0.0f;
	p_inside = false;

	for (const auto& opening : m_openings)
	{
		const float dx = p_x - opening.x, dz = p_z - opening.z;
		const float distance = std::sqrt(dx * dx + dz * dz);

		if (distance >= opening.radius)
		{
			continue;
		}

		p_inside = true;

		const float core = opening.radius * 0.7f;
		const float w = distance <= core ? 1.0f : 1.0f - SmoothStep((distance - core) / (opening.radius - core));
		weight = std::max(weight, w);
	}

	return weight;
}

float OvCore::Helpers::DistanceField::GetOpenness(const FVector3& p_point) const
{
	float openness = 0.0f;

	for (const auto& light : m_lights)
	{
		if (light.radius <= 0.0f)
		{
			continue;
		}

		const float distance = FVector3::Distance(p_point, light.position);

		if (distance < light.radius)
		{
			const float t = 1.0f - distance / light.radius;
			openness = std::max(openness, t * t);
		}
	}

	return openness;
}

float OvCore::Helpers::DistanceField::Evaluate(const FVector3& p_point, const std::vector<uint32_t>& p_candidates) const
{
	float value = kSolid;
	float nearest = kSolid;
	float floor = 0.0f;
	bool empty = false;

	for (const uint32_t index : p_candidates)
	{
		const auto& primitive = m_primitives[index];

		if (primitive.kind == Primitive::EKind::PILLAR || primitive.kind == Primitive::EKind::BOX)
		{
			continue;
		}

		float primitiveFloor = 0.0f;
		const float distance = GetDistance(primitive, p_point, &primitiveFloor);
		value = empty ? SmoothMin(value, distance, m_blend) : distance;
		empty = true;

		if (distance < nearest)
		{
			nearest = distance;
			floor = primitiveFloor;
		}
	}

	if (empty)
	{
		// The noise in full on the walls, a share of it on the floor of the nearest space, part of it folded into
		// ridges; the ledges on the walls only
		float wave = m_noiseAmplitude > 0.0f || m_ledgeDepth > 0.0f ? GetNoise(p_point) : 0.0f;

		if (m_ridgeShare > 0.0f)
		{
			wave = Mix(wave, 1.0f - 2.0f * std::abs(wave), m_ridgeShare);
		}

		const float noise = m_noiseAmplitude * wave;
		const float onFloor = std::clamp((p_point.y - floor) / 1.5f, 0.0f, 1.0f);

		value += noise * Mix(m_floorShare, 1.0f, onFloor) + GetRelief(p_point, wave) * onFloor;

		for (const uint32_t index : p_candidates)
		{
			const auto& primitive = m_primitives[index];

			if (primitive.kind == Primitive::EKind::PILLAR && Inside(p_point, primitive.boundsMin - FVector3{ m_noiseAmplitude, 0.0f, m_noiseAmplitude }, primitive.boundsMax + FVector3{ m_noiseAmplitude, 0.0f, m_noiseAmplitude }))
			{
				value = std::max(value, -(GetDistance(primitive, p_point) + noise));
			}
		}
	}

	// The boxes as they are, sharp, over the rest; under a box's floor, the solid it stands on kept (a tunnel ending
	// at its door does not eat into it)
	for (const uint32_t index : p_candidates)
	{
		const auto& primitive = m_primitives[index];

		if (primitive.kind == Primitive::EKind::BOX && p_point.x > primitive.a.x && p_point.x < primitive.b.x && p_point.z > primitive.a.z && p_point.z < primitive.b.z)
		{
			float floor = 0.0f;
			GetDistance(primitive, p_point, &floor);

			if (p_point.y < floor && p_point.y > floor - kKeptUnder)
			{
				value = std::max(value, floor - p_point.y);
			}
		}
	}

	for (const uint32_t index : p_candidates)
	{
		const auto& primitive = m_primitives[index];

		if (primitive.kind == Primitive::EKind::BOX)
		{
			const float distance = GetDistance(primitive, p_point);

			value = empty ? std::min(value, distance) : distance;
			empty = true;
		}
	}

	if (!m_ground.empty())
	{
		const float ground = GetGround(p_point.x, p_point.z);
		bool inside = false;
		const float opening = GetOpening(p_point.x, p_point.z, inside);

		// The empty spaces under their cover of solid, lifted at an opening; the air above an opening's ground
		value = std::max(value, p_point.y - (ground - m_cover * (1.0f - opening)));

		if (inside)
		{
			value = std::min(value, (ground - m_sink) - p_point.y);
		}
	}

	return value;
}

float OvCore::Helpers::DistanceField::GetValue(const FVector3& p_point) const
{
	BuildGrid();

	const auto found = m_grid.find(CellKey(CellOf(p_point.x), CellOf(p_point.y), CellOf(p_point.z)));
	static const std::vector<uint32_t> none;

	return Evaluate(p_point, found != m_grid.end() ? found->second : none);
}

std::optional<float> OvCore::Helpers::DistanceField::FindFloor(const FVector3& p_point, float p_depth) const
{
	constexpr float kStep = 0.5f;
	constexpr int kRefinements = 8;

	if (GetValue(p_point) >= 0.0f)
	{
		return std::nullopt;
	}

	float above = p_point.y;

	for (float travelled = kStep; travelled <= p_depth + kStep * 0.5f; travelled += kStep)
	{
		const float below = p_point.y - std::min(travelled, p_depth);

		if (GetValue({ p_point.x, below, p_point.z }) >= 0.0f)
		{
			float solid = below, empty = above;

			for (int refinement = 0; refinement < kRefinements; ++refinement)
			{
				const float middle = (solid + empty) * 0.5f;

				if (GetValue({ p_point.x, middle, p_point.z }) >= 0.0f)
				{
					solid = middle;
				}
				else
				{
					empty = middle;
				}
			}

			return (solid + empty) * 0.5f;
		}

		above = below;
	}

	return std::nullopt;
}

std::optional<OvCore::Helpers::MeshData> OvCore::Helpers::DistanceField::Mesh(int32_t p_x, int32_t p_y, int32_t p_z, uint32_t p_cells, float p_voxel, std::vector<Sample>* p_samples, float p_sampleSpacing) const
{
	if (p_cells == 0 || p_voxel <= 0.0f)
	{
		return std::nullopt;
	}

	const int32_t n = static_cast<int32_t>(p_cells);
	const FVector3 low{ (p_x - 2) * p_voxel, (p_y - 2) * p_voxel, (p_z - 2) * p_voxel };
	const FVector3 high{ (p_x + n + 2) * p_voxel, (p_y + n + 2) * p_voxel, (p_z + n + 2) * p_voxel };

	std::vector<uint32_t> candidates;
	Gather(low, high, candidates);

	// An opening's air meets the ground only where the ground crosses the block
	bool opening = false;

	if (!m_ground.empty())
	{
		for (const auto& disc : m_openings)
		{
			if (disc.x + disc.radius < low.x || disc.x - disc.radius > high.x || disc.z + disc.radius < low.z || disc.z - disc.radius > high.z)
			{
				continue;
			}

			const float ground = GetGround(std::clamp(disc.x, low.x, high.x), std::clamp(disc.z, low.z, high.z));
			const float reach = disc.radius + m_groundSpacing * 4.0f;

			if (ground + reach >= low.y && ground - reach <= high.y)
			{
				opening = true;
			}
		}
	}

	if (candidates.empty() && !opening)
	{
		return std::nullopt;
	}

	// Samples from voxel -1 to n + 1 of the block, cells from -1 to n (index 0 is voxel -1)
	const int32_t side = n + 3;
	const auto at = [side](int32_t i, int32_t j, int32_t k) { return (static_cast<size_t>(k) * side + j) * side + i; };
	const auto position = [&](int32_t i, int32_t j, int32_t k) {
		return FVector3{ (p_x + i - 1) * p_voxel, (p_y + j - 1) * p_voxel, (p_z + k - 1) * p_voxel };
	};

	std::vector<float> values(static_cast<size_t>(side) * side * side);
	bool anyEmpty = false, anySolid = false;

	for (int32_t k = 0; k < side; ++k)
	{
		for (int32_t j = 0; j < side; ++j)
		{
			for (int32_t i = 0; i < side; ++i)
			{
				const float value = Evaluate(position(i, j, k), candidates);
				values[at(i, j, k)] = value;
				(value < 0.0f ? anyEmpty : anySolid) = true;
			}
		}
	}

	if (!anyEmpty || !anySolid)
	{
		return std::nullopt;
	}

	static constexpr std::array<std::array<int32_t, 3>, 8> kCorners = { {
		{ 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, { 1, 1, 0 }, { 0, 0, 1 }, { 1, 0, 1 }, { 0, 1, 1 }, { 1, 1, 1 }
	} };

	static constexpr std::array<std::array<int32_t, 2>, 12> kEdges = { {
		{ 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
	} };

	// A vertex in each cell the surface crosses, where its edges cross it on average
	const int32_t cells = side - 1;
	const auto cellAt = [cells](int32_t i, int32_t j, int32_t k) { return (static_cast<size_t>(k) * cells + j) * cells + i; };
	std::vector<int32_t> cellVertex(static_cast<size_t>(cells) * cells * cells, -1);

	MeshData mesh;
	const float epsilon = p_voxel * 0.5f;

	for (int32_t k = 0; k < cells; ++k)
	{
		for (int32_t j = 0; j < cells; ++j)
		{
			for (int32_t i = 0; i < cells; ++i)
			{
				std::array<float, 8> corner;
				uint32_t mask = 0;

				for (size_t c = 0; c < 8; ++c)
				{
					corner[c] = values[at(i + kCorners[c][0], j + kCorners[c][1], k + kCorners[c][2])];
					mask |= (corner[c] < 0.0f ? 1u : 0u) << c;
				}

				if (mask == 0 || mask == 255)
				{
					continue;
				}

				FVector3 sum = FVector3::Zero;
				float crossings = 0.0f;

				for (const auto& edge : kEdges)
				{
					const float a = corner[edge[0]], b = corner[edge[1]];

					if ((a < 0.0f) == (b < 0.0f))
					{
						continue;
					}

					const float t = a / (a - b);
					const auto& ca = kCorners[edge[0]];
					const auto& cb = kCorners[edge[1]];
					sum += FVector3{
						Mix(static_cast<float>(ca[0]), static_cast<float>(cb[0]), t),
						Mix(static_cast<float>(ca[1]), static_cast<float>(cb[1]), t),
						Mix(static_cast<float>(ca[2]), static_cast<float>(cb[2]), t)
					};
					crossings += 1.0f;
				}

				const FVector3 local = sum / crossings;
				const FVector3 point = position(i, j, k) + local * p_voxel;

				// The normal points down the field, towards the empty side
				const FVector3 gradient{
					Evaluate(point + FVector3{ epsilon, 0.0f, 0.0f }, candidates) - Evaluate(point - FVector3{ epsilon, 0.0f, 0.0f }, candidates),
					Evaluate(point + FVector3{ 0.0f, epsilon, 0.0f }, candidates) - Evaluate(point - FVector3{ 0.0f, epsilon, 0.0f }, candidates),
					Evaluate(point + FVector3{ 0.0f, 0.0f, epsilon }, candidates) - Evaluate(point - FVector3{ 0.0f, 0.0f, epsilon }, candidates)
				};
				const float length = FVector3::Length(gradient);
				const FVector3 normal = length > 0.0f ? gradient / -length : FVector3::Up;

				OvRendering::Geometry::Vertex vertex{};
				vertex.position[0] = point.x;
				vertex.position[1] = point.y;
				vertex.position[2] = point.z;
				vertex.normals[0] = normal.x;
				vertex.normals[1] = normal.y;
				vertex.normals[2] = normal.z;
				vertex.texCoords[0] = m_ground.empty() ? 0.0f : GetGround(point.x, point.z) - point.y;
				vertex.texCoords[1] = GetOpenness(point);

				cellVertex[cellAt(i, j, k)] = static_cast<int32_t>(mesh.vertices.size());
				mesh.vertices.push_back(vertex);
			}
		}
	}

	const auto vertexPosition = [&mesh](uint32_t p_vertex) {
		const auto& v = mesh.vertices[p_vertex].position;
		return FVector3{ v[0], v[1], v[2] };
	};

	// Faces towards the empty side: the solid end of the crossed edge looks along p_facing
	const auto quad = [&](int32_t a, int32_t b, int32_t c, int32_t d, const FVector3& p_facing) {
		if (a < 0 || b < 0 || c < 0 || d < 0)
		{
			return;
		}

		const uint32_t q[4] = { static_cast<uint32_t>(a), static_cast<uint32_t>(b), static_cast<uint32_t>(c), static_cast<uint32_t>(d) };
		const bool alongFirst = FVector3::Distance(vertexPosition(q[0]), vertexPosition(q[2])) <= FVector3::Distance(vertexPosition(q[1]), vertexPosition(q[3]));
		std::array<uint32_t, 6> triangles = alongFirst ?
			std::array<uint32_t, 6>{ q[0], q[1], q[2], q[0], q[2], q[3] } :
			std::array<uint32_t, 6>{ q[0], q[1], q[3], q[1], q[2], q[3] };

		const FVector3 p0 = vertexPosition(triangles[0]);
		const FVector3 facing = FVector3::Cross(vertexPosition(triangles[1]) - p0, vertexPosition(triangles[2]) - p0);

		if (FVector3::Dot(facing, p_facing) < 0.0f)
		{
			std::swap(triangles[1], triangles[2]);
			std::swap(triangles[4], triangles[5]);
		}

		for (size_t t = 0; t < 6; t += 3)
		{
			// Over a ground, nothing stands above it: the air of an opening is not a wall
			if (!m_ground.empty())
			{
				const FVector3 centre = (vertexPosition(triangles[t]) + vertexPosition(triangles[t + 1]) + vertexPosition(triangles[t + 2])) / 3.0f;

				if (centre.y > GetGround(centre.x, centre.z) + kAboveGround)
				{
					continue;
				}
			}

			mesh.indices.insert(mesh.indices.end(), { triangles[t], triangles[t + 1], triangles[t + 2] });
		}
	};

	// The edges the block owns: from voxels 0 to n - 1 (sample indices 1 to n) along every axis
	for (int32_t k = 1; k <= n; ++k)
	{
		for (int32_t j = 1; j <= n; ++j)
		{
			for (int32_t i = 1; i <= n; ++i)
			{
				const float here = values[at(i, j, k)];
				const bool solid = here >= 0.0f;

				if (solid != (values[at(i + 1, j, k)] >= 0.0f))
				{
					quad(cellVertex[cellAt(i, j - 1, k - 1)], cellVertex[cellAt(i, j, k - 1)], cellVertex[cellAt(i, j, k)], cellVertex[cellAt(i, j - 1, k)], solid ? FVector3::Right : FVector3::Right * -1.0f);
				}

				if (solid != (values[at(i, j + 1, k)] >= 0.0f))
				{
					quad(cellVertex[cellAt(i - 1, j, k - 1)], cellVertex[cellAt(i, j, k - 1)], cellVertex[cellAt(i, j, k)], cellVertex[cellAt(i - 1, j, k)], solid ? FVector3::Up : FVector3::Up * -1.0f);
				}

				if (solid != (values[at(i, j, k + 1)] >= 0.0f))
				{
					quad(cellVertex[cellAt(i - 1, j - 1, k)], cellVertex[cellAt(i, j - 1, k)], cellVertex[cellAt(i, j, k)], cellVertex[cellAt(i - 1, j, k)], solid ? FVector3::Forward : FVector3::Forward * -1.0f);
				}
			}
		}
	}

	if (mesh.indices.empty())
	{
		return std::nullopt;
	}

	// Only the vertices the triangles use
	std::vector<int32_t> remap(mesh.vertices.size(), -1);
	std::vector<OvRendering::Geometry::Vertex> used;
	used.reserve(mesh.vertices.size());

	for (uint32_t& index : mesh.indices)
	{
		if (remap[index] < 0)
		{
			remap[index] = static_cast<int32_t>(used.size());
			used.push_back(mesh.vertices[index]);
		}

		index = static_cast<uint32_t>(remap[index]);
	}

	mesh.vertices = std::move(used);

	if (p_samples != nullptr && p_sampleSpacing > 0.0f)
	{
		std::unordered_set<uint64_t> taken;
		const FVector3 blockLow{ p_x * p_voxel, p_y * p_voxel, p_z * p_voxel };
		const FVector3 blockHigh{ (p_x + n) * p_voxel, (p_y + n) * p_voxel, (p_z + n) * p_voxel };

		for (const auto& vertex : mesh.vertices)
		{
			const FVector3 point{ vertex.position[0], vertex.position[1], vertex.position[2] };

			if (point.x < blockLow.x || point.x >= blockHigh.x || point.y < blockLow.y || point.y >= blockHigh.y || point.z < blockLow.z || point.z >= blockHigh.z)
			{
				continue;
			}

			const uint64_t key = CellKey(
				static_cast<int32_t>(std::floor(point.x / p_sampleSpacing)),
				static_cast<int32_t>(std::floor(point.y / p_sampleSpacing)),
				static_cast<int32_t>(std::floor(point.z / p_sampleSpacing))
			);

			if (taken.insert(key).second)
			{
				p_samples->push_back({ point, { vertex.normals[0], vertex.normals[1], vertex.normals[2] } });
			}
		}
	}

	return mesh;
}
