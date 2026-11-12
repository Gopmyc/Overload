/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

#include <doctest/doctest.h>

#include <OvCore/ECS/Actor.h>
#include <OvCore/ECS/Components/CPhysicalBox.h>
#include <OvCore/ECS/Components/CPhysicalHeightField.h>
#include <OvCore/Navigation/NavMesh.h>
#include <OvCore/Navigation/SceneGeometry.h>
#include <OvCore/SceneSystem/Scene.h>
#include <OvPhysics/Core/PhysicsEngine.h>

using OvCore::Navigation::NavGeometry;
using OvCore::Navigation::NavMesh;
using OvCore::Navigation::NavMeshSettings;
using OvMaths::FQuaternion;
using OvMaths::FVector3;

namespace SceneGeometry = OvCore::Navigation::SceneGeometry;

namespace
{
	/**
	* A floor 20 m square, its top at 0, cut across at z = 0 by a wall with a 2 m gap from x = 3 to x = 5
	*/
	NavGeometry WalledFloor()
	{
		NavGeometry geometry;
		SceneGeometry::AddBox(geometry, { 0.0f, -0.5f, 0.0f }, FQuaternion::Identity, { 10.0f, 0.5f, 10.0f });
		SceneGeometry::AddBox(geometry, { -3.5f, 1.0f, 0.0f }, FQuaternion::Identity, { 6.5f, 1.0f, 0.25f });
		SceneGeometry::AddBox(geometry, { 7.5f, 1.0f, 0.0f }, FQuaternion::Identity, { 2.5f, 1.0f, 0.25f });
		return geometry;
	}

	float Length(const std::vector<FVector3>& p_corners)
	{
		float length = 0.0f;
		for (size_t i = 1; i < p_corners.size(); ++i)
		{
			const FVector3 d{ p_corners[i].x - p_corners[i - 1].x, p_corners[i].y - p_corners[i - 1].y, p_corners[i].z - p_corners[i - 1].z };
			length += std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
		}
		return length;
	}

	/**
	* The x at which a path crosses z = 0, or nothing when it does not
	*/
	std::optional<float> CrossingX(const std::vector<FVector3>& p_corners)
	{
		for (size_t i = 1; i < p_corners.size(); ++i)
		{
			const auto& a = p_corners[i - 1];
			const auto& b = p_corners[i];
			if ((a.z < 0.0f) != (b.z < 0.0f))
			{
				const float t = -a.z / (b.z - a.z);
				return a.x + (b.x - a.x) * t;
			}
		}
		return std::nullopt;
	}
}

TEST_SUITE("Navigation")
{
	TEST_CASE("a path goes round a wall through its gap, kept the agent's radius from its edges")
	{
		NavMesh mesh;
		REQUIRE_FALSE(mesh.Build(WalledFloor(), {}).has_value());
		REQUIRE(mesh.IsBuilt());
		CHECK(mesh.GetPolygonCount() > 0);

		const auto path = mesh.FindPath({ 0.0f, 0.0f, -5.0f }, { 0.0f, 0.0f, 5.0f });
		REQUIRE(path.has_value());
		CHECK(path->complete);
		REQUIRE(path->corners.size() >= 3);

		// It starts and ends where asked, on the floor
		CHECK(path->corners.front().x == doctest::Approx(0.0f).epsilon(0.05));
		CHECK(path->corners.front().z == doctest::Approx(-5.0f).epsilon(0.05));
		CHECK(path->corners.back().z == doctest::Approx(5.0f).epsilon(0.05));
		for (const auto& corner : path->corners)
		{
			CHECK(std::abs(corner.y) < 0.3f);
		}

		// Through the gap, no nearer than the radius to its sides, so longer than the way straight through the wall
		const auto crossing = CrossingX(path->corners);
		REQUIRE(crossing.has_value());
		CHECK(*crossing >= 3.0f + 0.4f - 0.25f);
		CHECK(*crossing <= 5.0f - 0.4f + 0.25f);
		CHECK(Length(path->corners) > 10.5f);
	}

	TEST_CASE("a place that cannot be reached gives the way to the nearest point, as incomplete")
	{
		NavGeometry geometry = WalledFloor();
		SceneGeometry::AddBox(geometry, { 30.0f, -0.5f, 0.0f }, FQuaternion::Identity, { 5.0f, 0.5f, 5.0f });

		NavMesh mesh;
		REQUIRE_FALSE(mesh.Build(geometry, {}).has_value());

		const auto path = mesh.FindPath({ 0.0f, 0.0f, -5.0f }, { 30.0f, 0.0f, 0.0f });
		REQUIRE(path.has_value());
		CHECK_FALSE(path->complete);
		CHECK(path->corners.back().x < 10.5f);

		// A point far from any polygon gives no path at all
		CHECK_FALSE(mesh.FindPath({ 0.0f, 0.0f, -5.0f }, { 0.0f, 50.0f, 0.0f }).has_value());
	}

	TEST_CASE("the nearest point is on the floor, and none is found beyond the extents")
	{
		NavMesh mesh;
		REQUIRE_FALSE(mesh.Build(WalledFloor(), {}).has_value());

		const auto nearest = mesh.FindNearestPoint({ -4.0f, 1.5f, -5.0f });
		REQUIRE(nearest.has_value());
		CHECK(nearest->y == doctest::Approx(0.0f).epsilon(0.2));
		CHECK(nearest->x == doctest::Approx(-4.0f).epsilon(0.01));

		// Into the wall: out of it, the radius away
		const auto beside = mesh.FindNearestPoint({ -4.0f, 0.0f, 0.0f });
		REQUIRE(beside.has_value());
		CHECK(std::abs(beside->z) >= 0.25f + 0.4f - 0.25f);

		CHECK_FALSE(mesh.FindNearestPoint({ 0.0f, 20.0f, 0.0f }).has_value());
		CHECK(mesh.FindNearestPoint({ 0.0f, 20.0f, 0.0f }, { 2.0f, 30.0f, 2.0f }).has_value());
	}

	TEST_CASE("a walk along the mesh stops at the wall, and goes through the gap")
	{
		NavMesh mesh;
		REQUIRE_FALSE(mesh.Build(WalledFloor(), {}).has_value());

		const auto blocked = mesh.Raycast({ 0.0f, 0.0f, -5.0f }, { 0.0f, 0.0f, 5.0f });
		REQUIRE(blocked.has_value());
		CHECK(blocked->hit);
		CHECK(blocked->fraction < 0.5f);
		CHECK(blocked->point.z == doctest::Approx(-0.65f).epsilon(0.4));
		CHECK(blocked->normal.z < -0.9f);

		const auto through = mesh.Raycast({ 4.0f, 0.0f, -5.0f }, { 4.0f, 0.0f, 5.0f });
		REQUIRE(through.has_value());
		CHECK_FALSE(through->hit);
		CHECK(through->point.z == doctest::Approx(5.0f).epsilon(0.05));

		CHECK_FALSE(mesh.Raycast({ 0.0f, 50.0f, 0.0f }, { 0.0f, 50.0f, 5.0f }).has_value());
	}

	TEST_CASE("a step higher than the climb cannot be walked up, a lower one can")
	{
		NavGeometry geometry;
		SceneGeometry::AddBox(geometry, { 0.0f, -0.5f, 0.0f }, FQuaternion::Identity, { 10.0f, 0.5f, 10.0f });
		SceneGeometry::AddBox(geometry, { -5.0f, 0.15f, 0.0f }, FQuaternion::Identity, { 2.0f, 0.15f, 2.0f });	// 0.3 m
		SceneGeometry::AddBox(geometry, { 5.0f, 0.4f, 0.0f }, FQuaternion::Identity, { 2.0f, 0.4f, 2.0f });	// 0.8 m

		NavMesh mesh;
		REQUIRE_FALSE(mesh.Build(geometry, {}).has_value());

		const auto low = mesh.FindPath({ 0.0f, 0.0f, -8.0f }, { -5.0f, 0.3f, 0.0f });
		REQUIRE(low.has_value());
		CHECK(low->complete);
		CHECK(low->corners.back().y == doctest::Approx(0.3f).epsilon(0.2));

		const auto high = mesh.FindPath({ 0.0f, 0.0f, -8.0f }, { 5.0f, 0.8f, 0.0f });
		REQUIRE(high.has_value());
		CHECK_FALSE(high->complete);

		// A taller climb takes the agent up
		NavMeshSettings climber;
		climber.agentMaxClimb = 1.0f;
		REQUIRE_FALSE(mesh.Build(geometry, climber).has_value());
		CHECK(mesh.FindPath({ 0.0f, 0.0f, -8.0f }, { 5.0f, 0.8f, 0.0f })->complete);
	}

	TEST_CASE("the tiles a box touches are built again, the rest left as it was")
	{
		NavMeshSettings settings;
		settings.tileSize = 32;

		NavMesh mesh;
		NavGeometry geometry = WalledFloor();
		REQUIRE_FALSE(mesh.Build(geometry, settings).has_value());
		const uint32_t tiles = mesh.GetTileCount();
		CHECK(tiles > 4);
		REQUIRE(mesh.FindPath({ 0.0f, 0.0f, -5.0f }, { 0.0f, 0.0f, 5.0f })->complete);

		// A door shuts the gap
		SceneGeometry::AddBox(geometry, { 4.0f, 1.0f, 0.0f }, FQuaternion::Identity, { 1.0f, 1.0f, 0.25f });
		REQUIRE_FALSE(mesh.RebuildTiles(geometry, { 3.0f, 0.0f, -0.25f }, { 5.0f, 2.0f, 0.25f }).has_value());

		CHECK_FALSE(mesh.FindPath({ 0.0f, 0.0f, -5.0f }, { 0.0f, 0.0f, 5.0f })->complete);
		CHECK(mesh.GetTileCount() == tiles);

		// Nothing to rebuild before a build, nor around a box out of the mesh
		NavMesh empty;
		CHECK(empty.RebuildTiles(geometry, FVector3::Zero, FVector3::One).has_value());
		CHECK_FALSE(mesh.RebuildTiles(geometry, { 100.0f, 0.0f, 100.0f }, { 101.0f, 1.0f, 101.0f }).has_value());
	}

	TEST_CASE("random points are on the mesh, the same for a seed, and within a radius when asked")
	{
		NavMesh mesh;
		CHECK_FALSE(mesh.GetRandomPoint().has_value());
		REQUIRE_FALSE(mesh.Build(WalledFloor(), {}).has_value());

		mesh.SetSeed(11);
		std::vector<FVector3> first;
		for (int i = 0; i < 20; ++i)
		{
			const auto point = mesh.GetRandomPoint();
			REQUIRE(point.has_value());
			CHECK(std::abs(point->y) < 0.3f);
			CHECK(std::abs(point->x) <= 10.0f);
			first.push_back(*point);
		}

		mesh.SetSeed(11);
		for (const auto& expected : first)
		{
			const auto again = mesh.GetRandomPoint();
			CHECK(again->x == doctest::Approx(expected.x));
			CHECK(again->z == doctest::Approx(expected.z));
		}

		for (int i = 0; i < 20; ++i)
		{
			const auto around = mesh.GetRandomPointAround({ -5.0f, 0.0f, -5.0f }, 2.0f);
			REQUIRE(around.has_value());
			const float dx = around->x + 5.0f;
			const float dz = around->z + 5.0f;
			CHECK(std::sqrt(dx * dx + dz * dz) <= 2.0f + 1e-4f);
		}
	}

	TEST_CASE("settings are brought into their ranges, and nothing builds from nothing or from too many tiles")
	{
		NavMeshSettings wild;
		wild.agentRadius = -1.0f;
		wild.agentMaxSlope = 120.0f;
		wild.cellSize = 0.0f;
		wild.tileSize = 4;
		wild.agentHeight = std::nanf("");
		const auto sane = NavMesh::Sanitize(wild);
		CHECK(sane.agentRadius == 0.0f);
		CHECK(sane.agentMaxSlope == 89.0f);
		CHECK(sane.cellSize == doctest::Approx(0.01f));
		CHECK(sane.tileSize == 16);
		CHECK(sane.agentHeight == doctest::Approx(1.8f));

		NavMesh mesh;
		const auto nothing = mesh.Build({}, {});
		REQUIRE(nothing.has_value());
		CHECK(nothing->find("no geometry") != std::string::npos);
		CHECK_FALSE(mesh.IsBuilt());

		NavGeometry huge;
		SceneGeometry::AddBox(huge, FVector3::Zero, FQuaternion::Identity, { 2000.0f, 0.5f, 2000.0f });
		NavMeshSettings fine;
		fine.cellSize = 0.05f;
		fine.tileSize = 16;
		const auto tooMany = mesh.Build(huge, fine);
		REQUIRE(tooMany.has_value());
		CHECK(tooMany->find("tiles") != std::string::npos);
		CHECK_FALSE(mesh.IsBuilt());
	}

	TEST_CASE("a scene gives its static colliders: not the moving ones, not triggers, only the layers asked")
	{
		using namespace OvCore::ECS::Components;

		// Colliders need a physics engine to make their shapes, freed after them
		OvPhysics::Core::PhysicsEngine engine{ {} };
		OvCore::SceneSystem::Scene scene("", "");

		const auto box = [&](const char* p_name, const FVector3& p_position, const FVector3& p_size) -> CPhysicalBox& {
			auto& actor = scene.CreateActor(p_name);
			actor.transform.SetLocalPosition(p_position);
			auto& body = actor.AddComponent<CPhysicalBox>();
			body.SetSize(p_size);
			body.SetStatic(true);
			return body;
		};

		box("Floor", { 0.0f, -0.5f, 0.0f }, { 10.0f, 0.5f, 10.0f });
		box("Moving", { 0.0f, 2.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }).SetStatic(false);
		box("Trigger", { 0.0f, 4.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }).SetTrigger(true);
		box("Layered", { 0.0f, 6.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }).SetCollisionLayer(3);
		box("Asleep", { 0.0f, 8.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }).owner.SetActive(false);

		auto& fieldActor = scene.CreateActor("Field");
		fieldActor.transform.SetLocalPosition({ 20.0f, 0.0f, 0.0f });
		REQUIRE(fieldActor.AddComponent<CPhysicalHeightField>().SetHeights(3, 2.0f, { 0, 0, 0, 0, 1, 0, 0, 0, 0 }));

		// The floor and the field: 12 triangles, and 2 per cell of a 3 by 3 grid
		const auto all = SceneGeometry::Collect(scene, 0xFFFFu);
		CHECK(all.GetTriangleCount() == 12 + 12 + 8);

		const auto withoutLayer3 = SceneGeometry::Collect(scene, 0xFFFFu & ~(1u << 3));
		CHECK(withoutLayer3.GetTriangleCount() == 12 + 8);

		// The field's middle sample is raised, at its place from the actor
		bool raised = false;
		for (size_t i = 0; i + 2 < withoutLayer3.vertices.size(); i += 3)
		{
			raised = raised || (withoutLayer3.vertices[i] == doctest::Approx(22.0f) && withoutLayer3.vertices[i + 1] == doctest::Approx(1.0f) && withoutLayer3.vertices[i + 2] == doctest::Approx(2.0f));
		}
		CHECK(raised);

		// And the floor's top is walked on
		NavMesh mesh;
		REQUIRE_FALSE(mesh.Build(withoutLayer3, {}).has_value());
		CHECK(mesh.FindPath({ -5.0f, 0.0f, -5.0f }, { 5.0f, 0.0f, 5.0f })->complete);
	}

	TEST_CASE("a box's faces and a capsule's face outwards, and hold the shape")
	{
		NavGeometry geometry;
		SceneGeometry::AddBox(geometry, { 1.0f, 2.0f, 3.0f }, FQuaternion({ 0.0f, 30.0f, 0.0f }), { 1.0f, 2.0f, 3.0f });
		SceneGeometry::AddCapsule(geometry, { 10.0f, 0.0f, 0.0f }, FQuaternion::Identity, 0.5f, 2.0f);

		const auto& v = geometry.vertices;
		const auto& indices = geometry.indices;
		bool outwards = true;
		float capsuleTop = -1000.0f;
		float capsuleSide = 0.0f;

		for (size_t t = 0; t + 2 < indices.size(); t += 3)
		{
			const FVector3 a{ v[indices[t] * 3], v[indices[t] * 3 + 1], v[indices[t] * 3 + 2] };
			const FVector3 b{ v[indices[t + 1] * 3], v[indices[t + 1] * 3 + 1], v[indices[t + 1] * 3 + 2] };
			const FVector3 c{ v[indices[t + 2] * 3], v[indices[t + 2] * 3 + 1], v[indices[t + 2] * 3 + 2] };
			const FVector3 e1{ b.x - a.x, b.y - a.y, b.z - a.z };
			const FVector3 e2{ c.x - a.x, c.y - a.y, c.z - a.z };
			const FVector3 normal{ e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x };
			const bool isCapsule = a.x > 5.0f;
			const FVector3 center = isCapsule ? FVector3{ 10.0f, std::clamp((a.y + b.y + c.y) / 3.0f, -1.0f, 1.0f), 0.0f } : FVector3{ 1.0f, 2.0f, 3.0f };
			const FVector3 out{ (a.x + b.x + c.x) / 3.0f - center.x, (a.y + b.y + c.y) / 3.0f - center.y, (a.z + b.z + c.z) / 3.0f - center.z };
			outwards = outwards && normal.x * out.x + normal.y * out.y + normal.z * out.z > 0.0f;

			if (isCapsule)
			{
				for (const auto* p : { &a, &b, &c })
				{
					capsuleTop = std::max(capsuleTop, p->y);
					capsuleSide = std::max(capsuleSide, std::abs(p->x - 10.0f));
				}
			}
		}

		CHECK(outwards);
		CHECK(capsuleTop >= 1.5f);
		CHECK(capsuleSide >= 0.5f);
	}
}
