/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cmath>

#include <OvCore/ECS/Actor.h>
#include <OvCore/ECS/Components/CPhysicalBox.h>
#include <OvCore/ECS/Components/CPhysicalCapsule.h>
#include <OvCore/ECS/Components/CPhysicalHeightField.h>
#include <OvCore/ECS/Components/CPhysicalSphere.h>
#include <OvCore/Navigation/SceneGeometry.h>
#include <OvCore/SceneSystem/Scene.h>
#include <OvMaths/FMatrix4.h>

namespace
{
	using OvMaths::FMatrix4;
	using OvMaths::FQuaternion;
	using OvMaths::FVector3;

	constexpr float kPi = 3.14159265358979f;
	constexpr int kSegments = 12;	// Around a sphere or a capsule
	constexpr int kRings = 6;		// From a pole to the equator

	FVector3 Subtract(const FVector3& p_a, const FVector3& p_b)
	{
		return { p_a.x - p_b.x, p_a.y - p_b.y, p_a.z - p_b.z };
	}

	FVector3 Cross(const FVector3& p_a, const FVector3& p_b)
	{
		return { p_a.y * p_b.z - p_a.z * p_b.y, p_a.z * p_b.x - p_a.x * p_b.z, p_a.x * p_b.y - p_a.y * p_b.x };
	}

	float Dot(const FVector3& p_a, const FVector3& p_b)
	{
		return p_a.x * p_b.x + p_a.y * p_b.y + p_a.z * p_b.z;
	}

	FVector3 Place(const FVector3& p_center, const FQuaternion& p_rotation, const FVector3& p_local)
	{
		const FVector3 rotated = FQuaternion::RotatePoint(p_local, p_rotation);
		return { p_center.x + rotated.x, p_center.y + rotated.y, p_center.z + rotated.z };
	}

	FVector3 Transform(const FMatrix4& p_matrix, const FVector3& p_point)
	{
		const float* m = p_matrix.data;
		return {
			m[0] * p_point.x + m[1] * p_point.y + m[2] * p_point.z + m[3],
			m[4] * p_point.x + m[5] * p_point.y + m[6] * p_point.z + m[7],
			m[8] * p_point.x + m[9] * p_point.y + m[10] * p_point.z + m[11]
		};
	}

	/**
	* Adds a triangle of a closed shape facing out: its normal away from the point inside the shape nearest to it
	*/
	void AddFacingOut(OvCore::Navigation::NavGeometry& p_geometry, const FVector3& p_a, const FVector3& p_b, const FVector3& p_c, const FVector3& p_inside)
	{
		const FVector3 normal = Cross(Subtract(p_b, p_a), Subtract(p_c, p_a));
		const FVector3 centroid{ (p_a.x + p_b.x + p_c.x) / 3.0f, (p_a.y + p_b.y + p_c.y) / 3.0f, (p_a.z + p_b.z + p_c.z) / 3.0f };

		if (Dot(normal, Subtract(centroid, p_inside)) >= 0.0f)
		{
			p_geometry.AddTriangle(p_a, p_b, p_c);
		}
		else
		{
			p_geometry.AddTriangle(p_a, p_c, p_b);
		}
	}
}

void OvCore::Navigation::SceneGeometry::AddBox(NavGeometry& p_geometry, const OvMaths::FVector3& p_center, const OvMaths::FQuaternion& p_rotation, const OvMaths::FVector3& p_halfExtents)
{
	FVector3 corners[8];
	for (int i = 0; i < 8; ++i)
	{
		corners[i] = Place(p_center, p_rotation, {
			(i & 1) ? p_halfExtents.x : -p_halfExtents.x,
			(i & 2) ? p_halfExtents.y : -p_halfExtents.y,
			(i & 4) ? p_halfExtents.z : -p_halfExtents.z
		});
	}

	// Each face, as two triangles of its four corners
	constexpr int kFaces[6][4] = {
		{ 0, 1, 3, 2 }, { 4, 5, 7, 6 },	// -Z, +Z
		{ 0, 1, 5, 4 }, { 2, 3, 7, 6 },	// -Y, +Y
		{ 0, 2, 6, 4 }, { 1, 3, 7, 5 }	// -X, +X
	};

	for (const auto& face : kFaces)
	{
		AddFacingOut(p_geometry, corners[face[0]], corners[face[1]], corners[face[2]], p_center);
		AddFacingOut(p_geometry, corners[face[0]], corners[face[2]], corners[face[3]], p_center);
	}
}

void OvCore::Navigation::SceneGeometry::AddCapsule(NavGeometry& p_geometry, const OvMaths::FVector3& p_center, const OvMaths::FQuaternion& p_rotation, float p_radius, float p_cylinderHeight)
{
	// The faces of a sphere of so many segments fall inside it: widened, they hold it
	const float radius = p_radius / (std::cos(kPi / kSegments) * std::cos(kPi / (4.0f * kRings)));
	const float half = std::max(p_cylinderHeight, 0.0f) * 0.5f;

	// Rings from the bottom pole to the top one; the equator twice, at either end of the cylinder
	std::vector<std::pair<float, float>> rings; // (radius, height)
	for (int ring = 0; ring <= kRings; ++ring)
	{
		const float latitude = -kPi * 0.5f + kPi * 0.5f * static_cast<float>(ring) / kRings;
		rings.emplace_back(radius * std::cos(latitude), radius * std::sin(latitude) - half);
	}
	for (int ring = 0; ring <= kRings; ++ring)
	{
		const float latitude = kPi * 0.5f * static_cast<float>(ring) / kRings;
		rings.emplace_back(radius * std::cos(latitude), radius * std::sin(latitude) + half);
	}

	const auto point = [&](size_t p_ring, int p_segment) {
		const float angle = 2.0f * kPi * static_cast<float>(p_segment % kSegments) / kSegments;
		return Place(p_center, p_rotation, { rings[p_ring].first * std::cos(angle), rings[p_ring].second, rings[p_ring].first * std::sin(angle) });
	};

	for (size_t ring = 0; ring + 1 < rings.size(); ++ring)
	{
		for (int segment = 0; segment < kSegments; ++segment)
		{
			const FVector3 a = point(ring, segment);
			const FVector3 b = point(ring, segment + 1);
			const FVector3 c = point(ring + 1, segment + 1);
			const FVector3 d = point(ring + 1, segment);

			// The axis point at the height of the band is inside, whatever the band
			const float height = (rings[ring].second + rings[ring + 1].second) * 0.5f;
			const FVector3 inside = Place(p_center, p_rotation, { 0.0f, std::clamp(height, -half, half), 0.0f });

			// At a pole, a ring is a point: one of the two triangles has no area
			if (rings[ring].first > 0.0f) AddFacingOut(p_geometry, a, b, c, inside);
			if (rings[ring + 1].first > 0.0f) AddFacingOut(p_geometry, a, c, d, inside);
		}
	}
}

void OvCore::Navigation::SceneGeometry::AddHeightField(NavGeometry& p_geometry, const OvMaths::FMatrix4& p_world, uint32_t p_sampleCount, float p_spacing, const std::vector<float>& p_heights)
{
	if (p_sampleCount < 2 || p_heights.size() < static_cast<size_t>(p_sampleCount) * p_sampleCount)
	{
		return;
	}

	const int first = static_cast<int>(p_geometry.vertices.size() / 3);

	for (uint32_t z = 0; z < p_sampleCount; ++z)
	{
		for (uint32_t x = 0; x < p_sampleCount; ++x)
		{
			const FVector3 world = Transform(p_world, { static_cast<float>(x) * p_spacing, p_heights[z * p_sampleCount + x], static_cast<float>(z) * p_spacing });
			p_geometry.vertices.push_back(world.x);
			p_geometry.vertices.push_back(world.y);
			p_geometry.vertices.push_back(world.z);
		}
	}

	const auto index = [&](uint32_t p_x, uint32_t p_z) { return first + static_cast<int>(p_z * p_sampleCount + p_x); };

	for (uint32_t z = 0; z + 1 < p_sampleCount; ++z)
	{
		for (uint32_t x = 0; x + 1 < p_sampleCount; ++x)
		{
			// Counter-clockwise seen from above: (x, z), (x + 1, z + 1), (x + 1, z), then (x, z), (x, z + 1), (x + 1, z + 1)
			p_geometry.indices.insert(p_geometry.indices.end(), { index(x, z), index(x + 1, z + 1), index(x + 1, z) });
			p_geometry.indices.insert(p_geometry.indices.end(), { index(x, z), index(x, z + 1), index(x + 1, z + 1) });
		}
	}
}

OvCore::Navigation::NavGeometry OvCore::Navigation::SceneGeometry::Collect(SceneSystem::Scene& p_scene, uint32_t p_layerMask)
{
	using namespace OvCore::ECS::Components;

	NavGeometry geometry;

	for (auto* actor : p_scene.GetActors())
	{
		if (!actor || !actor->IsActive())
		{
			continue;
		}

		auto* body = actor->GetComponent<CPhysicalObject>();
		if (!body || !body->IsStatic() || body->IsTrigger() || ((p_layerMask >> body->GetCollisionLayer()) & 1u) == 0)
		{
			continue;
		}

		const auto& transform = actor->transform;
		const FVector3& position = transform.GetWorldPosition();
		const FQuaternion& rotation = transform.GetWorldRotation();
		const FVector3& scale = transform.GetWorldScale();

		// Each shape as the physics makes it from the actor's scale (see OvPhysics' CreateShape)
		if (const auto* box = dynamic_cast<const CPhysicalBox*>(body))
		{
			const FVector3 size = box->GetSize();
			AddBox(geometry, position, rotation, { size.x * std::abs(scale.x), size.y * std::abs(scale.y), size.z * std::abs(scale.z) });
		}
		else if (const auto* sphere = dynamic_cast<const CPhysicalSphere*>(body))
		{
			const float largest = std::max({ std::abs(scale.x), std::abs(scale.y), std::abs(scale.z) });
			AddCapsule(geometry, position, rotation, sphere->GetRadius() * largest, 0.0f);
		}
		else if (const auto* capsule = dynamic_cast<const CPhysicalCapsule*>(body))
		{
			const float radius = capsule->GetRadius() * std::max(std::abs(scale.x), std::abs(scale.z));
			AddCapsule(geometry, position, rotation, radius, capsule->GetHeight() * std::abs(scale.y));
		}
		else if (const auto* field = dynamic_cast<const CPhysicalHeightField*>(body))
		{
			AddHeightField(geometry, transform.GetWorldMatrix(), field->GetSampleCount(), field->GetSpacing(), field->GetHeights());
		}
	}

	return geometry;
}
