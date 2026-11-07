/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

#include <doctest/doctest.h>

#include <OvMaths/FTransform.h>
#include <OvPhysics/Core/PhysicsEngine.h>
#include <OvPhysics/Entities/PhysicalBox.h>

using OvMaths::FQuaternion;
using OvMaths::FVector3;
using OvPhysics::Core::PhysicsEngine;
using OvPhysics::Entities::PhysicalBox;
using OvPhysics::Entities::PhysicalObject;
using OvPhysics::Entities::PhysicsHit;
using OvPhysics::Settings::PhysicsSettings;
using OvPhysics::Settings::QuerySettings;

namespace
{
	PhysicsSettings MakeSettings()
	{
		PhysicsSettings settings;
		settings.layerNames[2] = "Enemy";
		settings.layerNames[3] = "Floor";
		return settings;
	}

	/**
	* A physics world and its bodies, the bodies destroyed before the engine
	*/
	struct World
	{
		PhysicsEngine engine;
		std::vector<std::unique_ptr<OvMaths::FTransform>> transforms;
		std::vector<std::unique_ptr<PhysicalBox>> bodies;

		explicit World(const PhysicsSettings& p_settings = MakeSettings()) : engine(p_settings) {}

		~World()
		{
			bodies.clear();
		}

		PhysicalBox& AddBox(const FVector3& p_position, const FVector3& p_halfExtents, uint16_t p_layer, bool p_static = true, bool p_trigger = false)
		{
			auto& transform = *transforms.emplace_back(std::make_unique<OvMaths::FTransform>(p_position));
			auto& body = *bodies.emplace_back(std::make_unique<PhysicalBox>(transform, p_halfExtents));
			body.SetStatic(p_static);
			body.SetTrigger(p_trigger);
			body.SetLayer(p_layer);
			return body;
		}

		void Step(float p_seconds)
		{
			for (float time = 0.0f; time < p_seconds; time += 1.0f / 60.0f)
				engine.Update(1.0f / 60.0f);
		}
	};

	/**
	* A column under the origin: an enemy box, a trigger, a wall, then a wide floor
	*/
	struct Column : World
	{
		PhysicalBox& enemy = AddBox({ 0.0f, -5.0f, 0.0f }, { 0.5f, 0.5f, 0.5f }, 2);
		PhysicalBox& trigger = AddBox({ 0.0f, -10.0f, 0.0f }, { 0.5f, 0.5f, 0.5f }, 0, true, true);
		PhysicalBox& wall = AddBox({ 0.0f, -15.0f, 0.0f }, { 0.5f, 0.5f, 0.5f }, 0);
		PhysicalBox& floor = AddBox({ 0.0f, -20.0f, 0.0f }, { 2.0f, 0.5f, 2.0f }, 3);

		Column()
		{
			Step(1.0f / 60.0f);
		}
	};

	const FVector3 kDown = { 0.0f, -1.0f, 0.0f };

	void CheckHit(const PhysicsHit& p_hit, const PhysicalObject& p_object, float p_distance, const FVector3& p_normal)
	{
		CHECK(p_hit.object == &p_object);
		CHECK(p_hit.distance == doctest::Approx(p_distance).epsilon(0.001));
		CHECK(p_hit.normal.x == doctest::Approx(p_normal.x).epsilon(0.01));
		CHECK(p_hit.normal.y == doctest::Approx(p_normal.y).epsilon(0.01));
		CHECK(p_hit.normal.z == doctest::Approx(p_normal.z).epsilon(0.01));
	}

	bool Holds(const std::vector<PhysicalObject*>& p_objects, std::initializer_list<const PhysicalObject*> p_expected)
	{
		return p_objects.size() == p_expected.size() && std::all_of(p_expected.begin(), p_expected.end(), [&](const PhysicalObject* p_object)
		{
			return std::find(p_objects.begin(), p_objects.end(), p_object) != p_objects.end();
		});
	}
}

TEST_SUITE("Physics layers")
{
	TEST_CASE("layers are found by name and their pairs can be changed at run time")
	{
		World world;
		auto& engine = world.engine;

		CHECK(engine.FindLayer("Enemy") == 2);
		CHECK(engine.FindLayer("Floor") == 3);
		CHECK_FALSE(engine.FindLayer("Nope"));
		CHECK(engine.GetLayerName(15) == "Ragdoll");

		CHECK(engine.DoLayersCollide(2, 3));
		engine.SetLayersCollide(2, 3, false);
		CHECK_FALSE(engine.DoLayersCollide(2, 3));
		CHECK_FALSE(engine.DoLayersCollide(3, 2));
		engine.SetLayersCollide(3, 2, true);
		CHECK(engine.DoLayersCollide(2, 3));
	}

	TEST_CASE("bodies on layers that ignore each other go through")
	{
		auto settings = MakeSettings();
		settings.collisionMasks[1] = static_cast<uint16_t>(settings.collisionMasks[1] & ~(1u << 3));
		settings.collisionMasks[3] = static_cast<uint16_t>(settings.collisionMasks[3] & ~(1u << 1));
		World world(settings);

		world.AddBox({ 0.0f, 0.0f, 0.0f }, { 5.0f, 0.5f, 5.0f }, 3);
		auto& ghost = world.AddBox({ -2.0f, 2.0f, 0.0f }, { 0.25f, 0.25f, 0.25f }, 1, false);
		auto& crate = world.AddBox({ 2.0f, 2.0f, 0.0f }, { 0.25f, 0.25f, 0.25f }, 2, false);

		world.Step(2.0f);

		CHECK(ghost.GetTransform().GetWorldPosition().y < -1.0f);
		CHECK(crate.GetTransform().GetWorldPosition().y == doctest::Approx(0.75f).epsilon(0.05));
	}
}

TEST_SUITE("Physics queries")
{
	TEST_CASE("a ray meets the objects nearest first, each once, with point and normal")
	{
		Column column;
		const auto hits = column.engine.CastRay({ 0.0f, 0.0f, 0.0f }, kDown, 30.0f);

		REQUIRE(hits.size() == 3);
		CheckHit(hits[0], column.enemy, 4.5f, { 0.0f, 1.0f, 0.0f });
		CheckHit(hits[1], column.wall, 14.5f, { 0.0f, 1.0f, 0.0f });
		CheckHit(hits[2], column.floor, 19.5f, { 0.0f, 1.0f, 0.0f });
		CHECK(hits[2].point.y == doctest::Approx(-19.5f));
	}

	TEST_CASE("triggers come in when asked, and the mask picks the layers")
	{
		Column column;

		const auto withTriggers = column.engine.CastRay({ 0.0f, 0.0f, 0.0f }, kDown, 30.0f, { 0xFFFF, true });
		REQUIRE(withTriggers.size() == 4);
		CheckHit(withTriggers[1], column.trigger, 9.5f, { 0.0f, 1.0f, 0.0f });

		const auto floorOnly = column.engine.CastRay({ 0.0f, 0.0f, 0.0f }, kDown, 30.0f, { 1u << 3, false });
		REQUIRE(floorOnly.size() == 1);
		CHECK(floorOnly[0].object == &column.floor);

		const auto defaultAndEnemy = column.engine.CastRay({ 0.0f, 0.0f, 0.0f }, kDown, 30.0f, { (1u << 0) | (1u << 2), false });
		REQUIRE(defaultAndEnemy.size() == 2);
		CHECK(defaultAndEnemy[0].object == &column.enemy);
		CHECK(defaultAndEnemy[1].object == &column.wall);
	}

	TEST_CASE("a ray that starts inside an object meets it at distance 0")
	{
		Column column;
		const auto hits = column.engine.CastRay({ 0.0f, -15.0f, 0.0f }, kDown, 10.0f);

		REQUIRE(hits.size() == 2);
		CHECK(hits[0].object == &column.wall);
		CHECK(hits[0].distance == doctest::Approx(0.0f));
		CHECK(hits[1].object == &column.floor);
		CHECK(hits[1].distance == doctest::Approx(4.5f));
	}

	TEST_CASE("a side face gives a side normal")
	{
		Column column;
		const auto hits = column.engine.CastRay({ -10.0f, -15.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, 20.0f);

		REQUIRE(hits.size() == 1);
		CheckHit(hits[0], column.wall, 9.5f, { -1.0f, 0.0f, 0.0f });
	}

	TEST_CASE("bad input gives no hit")
	{
		Column column;
		const float nan = std::numeric_limits<float>::quiet_NaN();
		auto& engine = column.engine;

		CHECK(engine.CastRay({ 0.0f, 0.0f, 0.0f }, kDown, 3.0f).empty());
		CHECK(engine.CastRay({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 30.0f).empty());
		CHECK(engine.CastRay({ 0.0f, 0.0f, 0.0f }, kDown, 0.0f).empty());
		CHECK(engine.CastRay({ 0.0f, 0.0f, 0.0f }, { nan, -1.0f, 0.0f }, 30.0f).empty());
		CHECK(engine.CastSphere({ 0.0f, 0.0f, 0.0f }, 0.0f, kDown, 30.0f).empty());
		CHECK(engine.CastSphere({ 0.0f, 0.0f, 0.0f }, 0.25f, kDown, nan).empty());
		CHECK(engine.OverlapSphere({ 0.0f, -15.0f, 0.0f }, -1.0f).empty());
	}

	TEST_CASE("a sphere cast stops a radius short")
	{
		Column column;
		const auto hits = column.engine.CastSphere({ 0.0f, 0.0f, 0.0f }, 0.25f, kDown, 30.0f);

		REQUIRE(hits.size() == 3);
		CheckHit(hits[0], column.enemy, 4.25f, { 0.0f, 1.0f, 0.0f });
		CheckHit(hits[1], column.wall, 14.25f, { 0.0f, 1.0f, 0.0f });
		CheckHit(hits[2], column.floor, 19.25f, { 0.0f, 1.0f, 0.0f });

		const auto inside = column.engine.CastSphere({ 0.0f, -15.0f, 0.0f }, 0.25f, kDown, 10.0f);
		REQUIRE_FALSE(inside.empty());
		CHECK(inside[0].object == &column.wall);
		CHECK(inside[0].distance == doctest::Approx(0.0f));
	}

	TEST_CASE("overlaps return the objects a shape overlaps, each once")
	{
		Column column;
		auto& engine = column.engine;
		const FVector3 wallCenter = { 0.0f, -15.0f, 0.0f };

		CHECK(Holds(engine.OverlapSphere(wallCenter, 0.6f), { &column.wall }));
		CHECK(Holds(engine.OverlapSphere(wallCenter, 6.0f), { &column.wall, &column.floor }));
		CHECK(Holds(engine.OverlapSphere(wallCenter, 6.0f, { 0xFFFF, true }), { &column.wall, &column.floor, &column.trigger }));

		const FQuaternion identity;
		CHECK(engine.OverlapBox({ 0.0f, -10.0f, 0.0f }, { 0.1f, 0.1f, 0.1f }, identity).empty());
		CHECK(Holds(engine.OverlapBox({ 0.0f, -10.0f, 0.0f }, { 0.1f, 0.1f, 0.1f }, identity, { 0xFFFF, true }), { &column.trigger }));

		CHECK(Holds(engine.OverlapBox({ 1.2f, -15.0f, 0.0f }, { 0.8f, 0.05f, 0.05f }, identity), { &column.wall }));
		CHECK(engine.OverlapBox({ 1.2f, -15.0f, 0.0f }, { 0.8f, 0.05f, 0.05f }, FQuaternion({ 0.0f, 90.0f, 0.0f })).empty());
	}

	TEST_CASE("the former Raycast is unchanged")
	{
		Column column;
		const auto hit = column.engine.Raycast({ 0.0f, 0.0f, 0.0f }, kDown, 30.0f);

		REQUIRE(hit);
		CHECK(hit->ResultObjects.size() == 4); // triggers included, as before
	}
}

TEST_SUITE("Physics engine")
{
	TEST_CASE("an engine created after another is gone gets the new bodies")
	{
		// The first engine must stop listening for new bodies when it is destroyed: the box made afterwards would
		// otherwise reach it in freed memory (which AddressSanitizer reports)
		{
			World first;
			first.AddBox({ 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, 0);
		}

		World second;
		auto& box = second.AddBox({ 0.0f, -5.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, 0);
		second.Step(1.0f / 60.0f);

		const auto hits = second.engine.CastRay({ 0.0f, 0.0f, 0.0f }, kDown, 10.0f);
		REQUIRE(hits.size() == 1);
		CHECK(hits[0].object == &box);
	}
}
