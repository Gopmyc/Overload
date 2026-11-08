/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include <doctest/doctest.h>

#include <OvMaths/FTransform.h>
#include <OvPhysics/Core/PhysicsEngine.h>
#include <OvPhysics/Entities/CharacterController.h>
#include <OvPhysics/Entities/PhysicalBox.h>

using OvMaths::FQuaternion;
using OvMaths::FVector3;
using OvPhysics::Core::PhysicsEngine;
using OvPhysics::Entities::CharacterController;
using OvPhysics::Entities::PhysicalBox;
using OvPhysics::Entities::PhysicalObject;
using OvPhysics::Settings::PhysicsSettings;

namespace
{
	constexpr float kStep = 1.0f / 60.0f;

	// A character of the default shape stands this high above the ground: half its straight part plus its radius
	constexpr float kRadius = 0.4f;
	constexpr float kHeight = 1.0f;
	constexpr float kStandingHeight = kHeight * 0.5f + kRadius;

	/**
	* A physics world with characters and boxes, all destroyed before the engine
	*/
	struct World
	{
		PhysicsEngine engine;
		std::vector<std::unique_ptr<OvMaths::FTransform>> transforms;
		std::vector<std::unique_ptr<PhysicalObject>> objects;

		explicit World(const PhysicsSettings& p_settings = {}) : engine(p_settings) {}

		~World()
		{
			objects.clear();
		}

		OvMaths::FTransform& AddTransform(const FVector3& p_position, const FQuaternion& p_rotation = FQuaternion::Identity)
		{
			return *transforms.emplace_back(std::make_unique<OvMaths::FTransform>(p_position, p_rotation));
		}

		PhysicalBox& AddBox(const FVector3& p_position, const FVector3& p_halfExtents, const FQuaternion& p_rotation = FQuaternion::Identity, bool p_static = true)
		{
			auto box = std::make_unique<PhysicalBox>(AddTransform(p_position, p_rotation), p_halfExtents);
			box->SetStatic(p_static);
			auto& result = *box;
			objects.push_back(std::move(box));
			return result;
		}

		/**
		* A floor whose top is at y = 0, 40 m wide
		*/
		PhysicalBox& AddFloor(float p_top = 0.0f, float p_minX = -20.0f, float p_maxX = 20.0f)
		{
			return AddBox({ (p_minX + p_maxX) * 0.5f, p_top - 0.5f, 0.0f }, { (p_maxX - p_minX) * 0.5f, 0.5f, 20.0f });
		}

		CharacterController& AddCharacter(const FVector3& p_position)
		{
			auto character = std::make_unique<CharacterController>(AddTransform(p_position), kRadius, kHeight);
			auto& result = *character;
			objects.push_back(std::move(character));
			return result;
		}

		void Step(float p_seconds)
		{
			const int steps = static_cast<int>(std::lround(p_seconds / kStep));
			for (int step = 0; step < steps; ++step)
				engine.Update(kStep);
		}
	};

	FVector3 PositionOf(PhysicalObject& p_object)
	{
		return p_object.GetTransform().GetWorldPosition();
	}

	float HorizontalSpeed(const FVector3& p_velocity)
	{
		return std::sqrt(p_velocity.x * p_velocity.x + p_velocity.z * p_velocity.z);
	}
}

TEST_SUITE("Character controller")
{
	TEST_CASE("a character falls, lands and stands on the ground")
	{
		World world;
		auto& floor = world.AddFloor();
		auto& character = world.AddCharacter({ 0.0f, 3.0f, 0.0f });

		CHECK(character.IsCharacter());
		CHECK(character.IsKinematic());
		CHECK_FALSE(character.IsStatic());

		world.Step(0.3f);
		CHECK_FALSE(character.IsGrounded());
		CHECK(character.GetGroundState() == CharacterController::EGroundState::IN_AIR);
		CHECK(character.GetLinearVelocity().y < -2.0f);
		CHECK(character.GetRealVelocity().y == doctest::Approx(character.GetLinearVelocity().y).epsilon(0.05));

		world.Step(2.0f);
		CHECK(character.IsGrounded());
		CHECK(character.GetGroundObject() == &floor);
		CHECK(character.GetGroundNormal().y == doctest::Approx(1.0f).epsilon(0.001));
		CHECK(PositionOf(character).y == doctest::Approx(kStandingHeight).epsilon(0.01));
		CHECK(character.GetLinearVelocity().y == 0.0f);
		CHECK(std::abs(character.GetRealVelocity().y) < 0.01f);
	}

	TEST_CASE("it stands still on a slope it can walk, and slides down a steeper one")
	{
		SUBCASE("30 degrees: it stays where it lands")
		{
			World world;
			world.AddBox({ 0.0f, 0.0f, 0.0f }, { 6.0f, 0.5f, 6.0f }, FQuaternion({ 0.0f, 0.0f, 30.0f }));
			auto& character = world.AddCharacter({ 0.0f, 3.0f, 0.0f });

			world.Step(1.5f);
			REQUIRE(character.IsGrounded());
			const FVector3 landed = PositionOf(character);

			world.Step(2.0f);
			CHECK(character.IsGrounded());
			CHECK(std::abs(PositionOf(character).x - landed.x) < 0.01f);
			CHECK(std::abs(PositionOf(character).y - landed.y) < 0.01f);
		}

		SUBCASE("60 degrees, steeper than its 50: it slides down")
		{
			World world;
			world.AddBox({ 0.0f, 0.0f, 0.0f }, { 6.0f, 0.5f, 6.0f }, FQuaternion({ 0.0f, 0.0f, 60.0f }));
			auto& character = world.AddCharacter({ 0.0f, 3.0f, 0.0f });

			world.Step(0.6f);
			const FVector3 touched = PositionOf(character);

			world.Step(0.5f);
			CHECK_FALSE(character.IsGrounded());
			CHECK(PositionOf(character).y < touched.y - 0.3f);

			// Sliding speeds up as a body's would on a slope without friction, g sin(60) a second
			const FVector3 real = character.GetRealVelocity();
			CHECK(std::sqrt(real.x * real.x + real.y * real.y) > 3.0f);
		}

		SUBCASE("wedged between two steep slopes, it rests instead of gathering speed: Jolt counts a wedge as ground")
		{
			World world;
			world.AddBox({ -1.0f, 0.0f, 0.0f }, { 2.0f, 0.2f, 4.0f }, FQuaternion({ 0.0f, 0.0f, -60.0f }));
			world.AddBox({ 1.0f, 0.0f, 0.0f }, { 2.0f, 0.2f, 4.0f }, FQuaternion({ 0.0f, 0.0f, 60.0f }));
			auto& character = world.AddCharacter({ 0.0f, 3.0f, 0.0f });

			world.Step(2.0f);
			const FVector3 held = PositionOf(character);
			world.Step(1.0f);

			CHECK(character.IsGrounded());
			CHECK(std::abs(PositionOf(character).y - held.y) < 0.01f);
			CHECK(character.GetLinearVelocity().y > -0.5f);
		}
	}

	TEST_CASE("a wall stops it, its velocity stays the one it was given, and its real velocity says it does not move")
	{
		World world;
		world.AddFloor();
		world.AddBox({ 2.0f, 2.0f, 0.0f }, { 0.5f, 2.0f, 10.0f });
		auto& character = world.AddCharacter({ 0.0f, kStandingHeight, 0.0f });

		character.SetLinearVelocity({ 3.0f, 0.0f, 0.0f });
		world.Step(1.5f);

		// It keeps 2 cm from the wall
		CHECK(PositionOf(character).x == doctest::Approx(2.0f - 0.5f - kRadius - 0.02f).epsilon(0.005));
		CHECK(character.GetLinearVelocity().x == 3.0f);
		CHECK(std::abs(character.GetRealVelocity().x) < 0.05f);
		CHECK(character.IsGrounded());

		// Along the wall, it slides at the speed it was given that way
		character.SetLinearVelocity({ 3.0f, 0.0f, 2.0f });
		world.Step(0.5f);
		CHECK(character.GetRealVelocity().z == doctest::Approx(2.0f).epsilon(0.05));
		CHECK(std::abs(character.GetRealVelocity().x) < 0.05f);
	}

	TEST_CASE("it climbs a step no higher than its step height, and not a higher one")
	{
		SUBCASE("a 0.2 m step")
		{
			World world;
			world.AddFloor();
			world.AddBox({ 4.0f, 0.1f, 0.0f }, { 2.0f, 0.1f, 4.0f });
			auto& character = world.AddCharacter({ 0.0f, kStandingHeight, 0.0f });

			character.SetLinearVelocity({ 2.0f, 0.0f, 0.0f });
			world.Step(1.5f);

			CHECK(PositionOf(character).x > 2.5f);
			CHECK(PositionOf(character).y == doctest::Approx(0.2f + kStandingHeight).epsilon(0.02));
			CHECK(character.IsGrounded());
		}

		SUBCASE("a 0.6 m block")
		{
			World world;
			world.AddFloor();
			world.AddBox({ 4.0f, 0.3f, 0.0f }, { 2.0f, 0.3f, 4.0f });
			auto& character = world.AddCharacter({ 0.0f, kStandingHeight, 0.0f });

			character.SetLinearVelocity({ 2.0f, 0.0f, 0.0f });
			world.Step(1.5f);

			CHECK(PositionOf(character).x == doctest::Approx(2.0f - kRadius - 0.02f).epsilon(0.005));
			CHECK(PositionOf(character).y == doctest::Approx(kStandingHeight).epsilon(0.02));
		}
	}

	TEST_CASE("it keeps to the ground going down a small drop, and falls off a ledge")
	{
		SUBCASE("a 0.2 m drop")
		{
			World world;
			world.AddFloor(0.0f, -20.0f, 2.0f);
			world.AddFloor(-0.2f, 2.0f, 20.0f);
			auto& character = world.AddCharacter({ 0.0f, kStandingHeight, 0.0f });
			world.Step(0.1f);

			character.SetLinearVelocity({ 3.0f, 0.0f, 0.0f });

			int airborneSteps = 0;
			for (int step = 0; step < 90; ++step)
			{
				world.Step(kStep);
				airborneSteps += character.GetGroundState() == CharacterController::EGroundState::IN_AIR;
			}

			CHECK(airborneSteps == 0);
			CHECK(PositionOf(character).x > 4.0f);
			CHECK(PositionOf(character).y == doctest::Approx(-0.2f + kStandingHeight).epsilon(0.02));
		}

		SUBCASE("a 1 m ledge")
		{
			World world;
			world.AddFloor(0.0f, -20.0f, 2.0f);
			world.AddFloor(-1.0f, 2.0f, 20.0f);
			auto& character = world.AddCharacter({ 0.0f, kStandingHeight, 0.0f });
			world.Step(0.1f);

			character.SetLinearVelocity({ 3.0f, 0.0f, 0.0f });

			// Falling 1 m takes some 27 steps, part of them along the ledge's edge
			int unsupportedSteps = 0;
			for (int step = 0; step < 90; ++step)
			{
				world.Step(kStep);
				unsupportedSteps += !character.IsGrounded();
			}

			CHECK(unsupportedSteps > 20);
			CHECK(character.IsGrounded());
			CHECK(PositionOf(character).y == doctest::Approx(-1.0f + kStandingHeight).epsilon(0.02));
		}
	}

	TEST_CASE("a jump rises as high as its speed allows, a ceiling ends it at once, and it lands")
	{
		SUBCASE("in the open")
		{
			World world;
			world.AddFloor();
			auto& character = world.AddCharacter({ 0.0f, kStandingHeight, 0.0f });
			world.Step(0.1f);
			REQUIRE(character.IsGrounded());

			character.SetLinearVelocity({ 0.0f, 5.0f, 0.0f });

			float highest = 0.0f;
			for (int step = 0; step < 90; ++step)
			{
				world.Step(kStep);
				highest = std::max(highest, PositionOf(character).y);
			}

			// v^2 / 2g above where it stood
			CHECK(highest == doctest::Approx(kStandingHeight + 25.0f / (2.0f * 9.81f)).epsilon(0.05));
			CHECK(character.IsGrounded());
			CHECK(character.GetLinearVelocity().y == 0.0f);
		}

		SUBCASE("under a ceiling")
		{
			World world;
			world.AddFloor();
			world.AddBox({ 0.0f, 2.8f, 0.0f }, { 5.0f, 0.5f, 5.0f });
			auto& character = world.AddCharacter({ 0.0f, kStandingHeight, 0.0f });
			world.Step(0.1f);

			character.SetLinearVelocity({ 0.0f, 5.0f, 0.0f });

			// The head meets the ceiling's underside, at y = 2.3, when the character is 2.3 - 0.9 high
			float highest = 0.0f;
			int stepsAtTop = 0;
			for (int step = 0; step < 90; ++step)
			{
				world.Step(kStep);
				highest = std::max(highest, PositionOf(character).y);
				stepsAtTop += PositionOf(character).y > 2.3f - kStandingHeight - 0.03f;
			}

			CHECK(highest == doctest::Approx(2.3f - kStandingHeight).epsilon(0.02));
			CHECK(stepsAtTop <= 2);
			CHECK(character.IsGrounded());
		}
	}

	TEST_CASE("queries meet it, and it raises trigger and collision events")
	{
		World world;
		auto& floor = world.AddFloor();
		auto& wall = world.AddBox({ 4.0f, 2.0f, 0.0f }, { 0.5f, 2.0f, 10.0f });
		auto& trigger = world.AddBox({ 2.0f, 1.0f, 0.0f }, { 0.5f, 1.0f, 1.0f });
		trigger.SetTrigger(true);
		auto& character = world.AddCharacter({ 0.0f, kStandingHeight, 0.0f });

		int floorStarts = 0, wallStarts = 0, wallStays = 0, triggerStarts = 0, triggerStops = 0;
		character.CollisionStartEvent += [&](PhysicalObject& p_other)
		{
			floorStarts += &p_other == &floor;
			wallStarts += &p_other == &wall;
		};
		character.CollisionStayEvent += [&](PhysicalObject& p_other) { wallStays += &p_other == &wall; };
		trigger.TriggerStartEvent += [&](PhysicalObject& p_other) { triggerStarts += &p_other == &character; };
		trigger.TriggerStopEvent += [&](PhysicalObject& p_other) { triggerStops += &p_other == &character; };

		world.Step(0.1f);
		CHECK(floorStarts == 1);

		const auto hits = world.engine.CastRay({ -3.0f, kStandingHeight, 0.0f }, { 1.0f, 0.0f, 0.0f }, 10.0f);
		REQUIRE_FALSE(hits.empty());
		CHECK(hits.front().object == &character);
		CHECK(hits.front().distance == doctest::Approx(3.0f - kRadius).epsilon(0.01));

		// Through the trigger, then against the wall
		character.SetLinearVelocity({ 3.0f, 0.0f, 0.0f });
		world.Step(2.0f);

		CHECK(triggerStarts == 1);
		CHECK(triggerStops == 1);
		CHECK(wallStarts == 1);
		CHECK(wallStays > 10);
		CHECK(floorStarts == 1);
	}

	TEST_CASE("its layer decides what it stands on")
	{
		PhysicsSettings settings;
		settings.collisionMasks[1] = static_cast<uint16_t>(~(1u << 2));
		settings.collisionMasks[2] = static_cast<uint16_t>(~(1u << 1));

		World world(settings);
		world.AddFloor().SetLayer(2);
		auto& character = world.AddCharacter({ 0.0f, 2.0f, 0.0f });
		character.SetLayer(1);

		world.Step(1.0f);
		CHECK(PositionOf(character).y < -1.0f);
		CHECK_FALSE(character.IsGrounded());
	}

	TEST_CASE("its transform moved elsewhere takes it there, and a disabled character stays put")
	{
		World world;
		world.AddFloor();
		auto& character = world.AddCharacter({ 0.0f, kStandingHeight, 0.0f });
		world.Step(0.2f);

		character.GetTransform().SetLocalPosition({ 5.0f, kStandingHeight + 1.0f, 3.0f });
		world.Step(1.0f);
		CHECK(PositionOf(character).x == doctest::Approx(5.0f));
		CHECK(PositionOf(character).z == doctest::Approx(3.0f));
		CHECK(PositionOf(character).y == doctest::Approx(kStandingHeight).epsilon(0.01));
		CHECK(character.IsGrounded());

		character.SetEnabled(false);
		character.GetTransform().SetLocalPosition({ 5.0f, 4.0f, 3.0f });
		world.Step(0.5f);
		CHECK(PositionOf(character).y == doctest::Approx(4.0f));
	}

	TEST_CASE("an impulse or a force changes its velocity by its mass, and it pushes a dynamic box")
	{
		World world;
		world.AddFloor();
		auto& character = world.AddCharacter({ 0.0f, kStandingHeight, 0.0f });
		CHECK(character.GetMass() == 70.0f);

		character.AddImpulse({ 140.0f, 0.0f, 0.0f });
		CHECK(character.GetLinearVelocity().x == doctest::Approx(2.0f));

		character.SetLinearVelocity({ 0.0f, 0.0f, 0.0f });
		character.AddForce({ 0.0f, 0.0f, 70.0f * 60.0f });
		world.Step(kStep);
		CHECK(character.GetLinearVelocity().z == doctest::Approx(1.0f));
		world.Step(kStep);
		CHECK(character.GetLinearVelocity().z == doctest::Approx(1.0f));

		auto& box = world.AddBox({ 2.0f, 0.5f, 5.0f }, { 0.5f, 0.5f, 0.5f }, FQuaternion::Identity, false);
		box.SetMass(5.0f);
		world.Step(0.5f);
		const float boxStart = PositionOf(box).x;

		character.GetTransform().SetLocalPosition({ 0.0f, kStandingHeight, 5.0f });
		character.SetLinearVelocity({ 2.0f, 0.0f, 0.0f });
		world.Step(2.0f);
		CHECK(PositionOf(box).x > boxStart + 0.5f);
	}

	TEST_CASE("the settings it is given are kept within their ranges")
	{
		World world;
		auto& character = world.AddCharacter({ 0.0f, 0.0f, 0.0f });

		character.SetMaxSlopeAngle(120.0f);
		CHECK(character.GetMaxSlopeAngle() == 90.0f);
		character.SetMaxSlopeAngle(-5.0f);
		CHECK(character.GetMaxSlopeAngle() == 0.0f);
		character.SetStepHeight(-1.0f);
		CHECK(character.GetStepHeight() == 0.0f);
		character.SetGroundSnapDistance(std::nanf(""));
		CHECK(character.GetGroundSnapDistance() == doctest::Approx(0.3f));
		character.SetPushStrength(-3.0f);
		CHECK(character.GetPushStrength() == 0.0f);
		character.SetGravityFactor(0.5f);
		CHECK(character.GetGravityFactor() == 0.5f);

		character.SetTrigger(true);
		CHECK_FALSE(character.IsTrigger());
		character.SetStatic(true);
		CHECK_FALSE(character.IsStatic());

		character.SetLinearVelocity({ 1.0f, 0.0f, 0.0f });
		character.SetKinematic(false);
		character.SetKinematic(true);
		CHECK(character.IsKinematic());
		CHECK(character.GetLinearVelocity().x == 1.0f);
	}
}
