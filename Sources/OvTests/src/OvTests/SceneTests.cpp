/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <set>

#include <doctest/doctest.h>

#include <OvCore/ECS/Actor.h>
#include <OvCore/SceneSystem/Scene.h>

using OvCore::SceneSystem::Scene;

TEST_SUITE("Scene")
{
	TEST_CASE("each actor has a GUID of its own, by which it is found")
	{
		Scene scene("", "");
		std::set<uint64_t> guids;

		for (int index = 0; index < 50; ++index)
		{
			auto& actor = scene.CreateActor("Actor" + std::to_string(index));
			CHECK(guids.insert(actor.GetGUID()).second);
			CHECK(scene.FindActorByGUID(actor.GetGUID()) == &actor);
		}

		CHECK(scene.GetActors().size() == 50);
	}

	TEST_CASE("a destroyed actor and its children are freed when garbage is collected")
	{
		Scene scene("", "");
		auto& parent = scene.CreateActor("Parent");
		auto& child = scene.CreateActor("Child");
		auto& other = scene.CreateActor("Other");
		child.SetParent(parent);

		const uint64_t parentGUID = parent.GetGUID();
		const uint64_t childGUID = child.GetGUID();

		parent.MarkAsDestroy();
		CHECK_FALSE(child.IsAlive());
		CHECK(scene.GetActors().size() == 3); // still there until the end of the frame

		scene.CollectGarbages();
		CHECK(scene.GetActors().size() == 1);
		CHECK(scene.GetActors().front() == &other);
		CHECK(scene.FindActorByGUID(parentGUID) == nullptr);
		CHECK(scene.FindActorByGUID(childGUID) == nullptr);
		CHECK(scene.FindActorByName("Child") == nullptr);
	}

	TEST_CASE("an actor destroyed at once leaves the others in place")
	{
		Scene scene("", "");
		auto& first = scene.CreateActor("First");
		auto& second = scene.CreateActor("Second");
		const uint64_t firstGUID = first.GetGUID();

		CHECK(scene.DestroyActor(first));
		CHECK(scene.FindActorByGUID(firstGUID) == nullptr);
		CHECK(scene.FindActorByGUID(second.GetGUID()) == &second);
		CHECK(scene.GetActors().size() == 1);
	}
}
