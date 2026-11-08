/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <set>
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

namespace
{
	// Four frames a physics step
	constexpr float kFrame = 1.0f / 240.0f;

	/**
	* A physics world whose objects are destroyed before the engine
	*/
	struct World
	{
		PhysicsEngine engine{ {} };
		std::vector<std::unique_ptr<OvMaths::FTransform>> transforms;
		std::vector<std::unique_ptr<PhysicalObject>> objects;

		~World()
		{
			objects.clear();
		}

		OvMaths::FTransform& AddTransform(const FVector3& p_position)
		{
			return *transforms.emplace_back(std::make_unique<OvMaths::FTransform>(p_position));
		}

		PhysicalBox& AddBox(const FVector3& p_position, const FVector3& p_halfExtents, bool p_static)
		{
			auto box = std::make_unique<PhysicalBox>(AddTransform(p_position), p_halfExtents);
			box->SetStatic(p_static);
			auto& result = *box;
			objects.push_back(std::move(box));
			return result;
		}

		CharacterController& AddCharacter(const FVector3& p_position)
		{
			auto character = std::make_unique<CharacterController>(AddTransform(p_position), 0.4f, 1.0f);
			auto& result = *character;
			objects.push_back(std::move(character));
			return result;
		}

		/**
		* A frame as the game runs it: the physics, then the fixed update when it stepped, then the interpolation
		*/
		bool Frame(const std::function<void()>& p_fixedUpdate = {})
		{
			const bool stepped = engine.Update(kFrame);

			if (stepped && p_fixedUpdate)
			{
				p_fixedUpdate();
			}

			engine.Interpolate();
			return stepped;
		}
	};

	float HeightOf(PhysicalObject& p_object)
	{
		return p_object.GetTransform().GetWorldPosition().y;
	}
}

TEST_SUITE("Physics interpolation")
{
	TEST_CASE("an interpolated body moves every frame between its last two steps, and the fixed update sees the physics")
	{
		World world;
		auto& smooth = world.AddBox({ -2.0f, 10.0f, 0.0f }, { 0.5f, 0.5f, 0.5f }, false);
		auto& plain = world.AddBox({ 2.0f, 10.0f, 0.0f }, { 0.5f, 0.5f, 0.5f }, false);
		smooth.SetInterpolated(true);
		CHECK(smooth.IsInterpolated());
		CHECK_FALSE(plain.IsInterpolated());

		std::vector<float> steps;
		std::set<float> smoothHeights, plainHeights;
		int stepCount = 0;
		int outside = 0;

		for (int frame = 0; frame < 240; ++frame)
		{
			const bool stepped = world.Frame([&]
			{
				// The fixed update sees the same pose with or without interpolation
				CHECK(HeightOf(smooth) == doctest::Approx(HeightOf(plain)));
				steps.push_back(HeightOf(smooth));
				++stepCount;
			});

			// On screen, the interpolated body is between its last two steps, the other one at its last step
			if (steps.size() >= 2)
			{
				const float previous = steps[steps.size() - 2];
				const float last = steps.back();
				const float height = HeightOf(smooth);
				outside += height > std::max(previous, last) + 1e-4f || height < std::min(previous, last) - 1e-4f;
				CHECK(HeightOf(plain) == doctest::Approx(last));
			}

			smoothHeights.insert(HeightOf(smooth));
			plainHeights.insert(HeightOf(plain));
			(void)stepped;
		}

		CHECK(stepCount == doctest::Approx(60).epsilon(0.05));
		CHECK(outside == 0);

		// The plain body only moves on steps; the interpolated one moves on (nearly) every frame
		CHECK(plainHeights.size() <= static_cast<size_t>(stepCount) + 1);
		CHECK(smoothHeights.size() > static_cast<size_t>(stepCount) * 3);
	}

	TEST_CASE("moving an interpolated body teleports it, whether in the fixed update or elsewhere")
	{
		World world;
		auto& box = world.AddBox({ 0.0f, 10.0f, 0.0f }, { 0.5f, 0.5f, 0.5f }, false);
		box.SetInterpolated(true);

		for (int frame = 0; frame < 30; ++frame)
			world.Frame();

		// Between two steps: it is drawn from where it was put, never sliding back toward its former steps
		box.GetTransform().SetLocalPosition({ 0.0f, 20.0f, 0.0f });
		for (int frame = 0; frame < 12; ++frame)
		{
			world.Frame();
			CHECK(HeightOf(box) > 19.9f);
			CHECK(HeightOf(box) <= 20.0f);
		}

		// In the fixed update: the interpolation leaves it where the script put it
		bool moved = false;
		while (!moved)
		{
			world.Frame([&]
			{
				box.GetTransform().SetLocalPosition({ 0.0f, 30.0f, 0.0f });
				moved = true;
			});
		}
		CHECK(HeightOf(box) == 30.0f);

		world.Frame();
		CHECK(HeightOf(box) > 29.9f);
	}

	TEST_CASE("a character controller is interpolated on its position, and its rotation stays the script's")
	{
		World world;
		world.AddBox({ 0.0f, -0.5f, 0.0f }, { 20.0f, 0.5f, 20.0f }, true);
		auto& character = world.AddCharacter({ 0.0f, 0.9f, 0.0f });
		character.SetInterpolated(true);

		for (int frame = 0; frame < 20; ++frame)
			world.Frame();

		character.SetLinearVelocity({ 3.0f, 0.0f, 0.0f });
		const FQuaternion facing({ 0.0f, 90.0f, 0.0f });

		float lastX = character.GetTransform().GetWorldPosition().x;
		int stillFrames = 0;

		for (int frame = 0; frame < 120; ++frame)
		{
			// The script turns the actor every frame: that is no teleport
			character.GetTransform().SetLocalRotation(facing);
			world.Frame();

			// Drawn a step behind, the motion shows from the step after it starts: then it moves every frame
			const float x = character.GetTransform().GetWorldPosition().x;
			stillFrames += frame >= 8 && x <= lastX;
			lastX = x;

			const FQuaternion rotation = character.GetTransform().GetLocalRotation();
			CHECK(std::abs(FQuaternion::DotProduct(rotation, facing)) == doctest::Approx(1.0f));
		}

		CHECK(stillFrames == 0);
		CHECK(character.GetTransform().GetWorldPosition().y == doctest::Approx(0.9f).epsilon(0.01));
		CHECK(lastX > 1.3f);
	}

	TEST_CASE("a body the physics does not move is never interpolated")
	{
		World world;
		auto& kinematic = world.AddBox({ 0.0f, 5.0f, 0.0f }, { 0.5f, 0.5f, 0.5f }, false);
		kinematic.SetKinematic(true);
		kinematic.SetInterpolated(true);
		auto& still = world.AddBox({ 3.0f, 5.0f, 0.0f }, { 0.5f, 0.5f, 0.5f }, true);
		still.SetInterpolated(true);

		for (int frame = 0; frame < 40; ++frame)
		{
			const float height = 5.0f + static_cast<float>(frame) * 0.01f;
			kinematic.GetTransform().SetLocalPosition({ 0.0f, height, 0.0f });
			world.Frame();

			CHECK(HeightOf(kinematic) == height);
			CHECK(HeightOf(still) == 5.0f);
		}
	}
}
