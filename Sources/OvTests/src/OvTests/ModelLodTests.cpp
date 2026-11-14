/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <numbers>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <tinyxml2.h>

#include <OvCore/ECS/Actor.h>
#include <OvCore/ECS/Components/CModelRenderer.h>
#include <OvCore/Rendering/LodSelection.h>
#include <OvCore/SceneSystem/Scene.h>
#include <OvMaths/FMatrix4.h>

using namespace OvCore::Rendering;

namespace
{
	// A skeleton of bones only, named in order: all CompareBones looks at
	OvRendering::Animation::Skeleton MakeSkeleton(const std::vector<std::string>& p_bones)
	{
		OvRendering::Animation::Skeleton skeleton;
		for (const auto& name : p_bones)
		{
			skeleton.nodeByName[name] = static_cast<uint32_t>(skeleton.nodes.size());
			skeleton.boneByName[name] = static_cast<uint32_t>(skeleton.bones.size());
			skeleton.bones.push_back({ name, static_cast<uint32_t>(skeleton.nodes.size()) });
			skeleton.nodes.push_back({ name });
		}
		return skeleton;
	}
}

TEST_SUITE("Model LOD")
{
	TEST_CASE("A level is chosen by distance, each from its start, with a margin against flicker")
	{
		const std::vector<float> starts{ 10.0f, 30.0f, 60.0f };

		CHECK(LodSelection::SelectLevel(5.0f, starts, 1.0f, 0) == 0);
		CHECK(LodSelection::SelectLevel(15.0f, starts, 1.0f, 0) == 1);
		CHECK(LodSelection::SelectLevel(45.0f, starts, 1.0f, 0) == 2);
		CHECK(LodSelection::SelectLevel(100.0f, starts, 1.0f, 0) == 3);

		// Several levels at once, either way
		CHECK(LodSelection::SelectLevel(100.0f, starts, 1.0f, 0) == 3);
		CHECK(LodSelection::SelectLevel(1.0f, starts, 1.0f, 3) == 0);

		// Within a tenth of a boundary, the level chosen last stays
		CHECK(LodSelection::SelectLevel(9.5f, starts, 1.0f, 1) == 1);
		CHECK(LodSelection::SelectLevel(10.5f, starts, 1.0f, 0) == 0);
		CHECK(LodSelection::SelectLevel(8.9f, starts, 1.0f, 1) == 0);
		CHECK(LodSelection::SelectLevel(11.1f, starts, 1.0f, 0) == 1);
		CHECK(LodSelection::SelectLevel(28.0f, starts, 1.0f, 2) == 2);
		CHECK(LodSelection::SelectLevel(26.0f, starts, 1.0f, 2) == 1);

		// No lighter level, or a level chosen last beyond the levels there are
		CHECK(LodSelection::SelectLevel(1000.0f, {}, 1.0f, 0) == 0);
		CHECK(LodSelection::SelectLevel(45.0f, starts, 1.0f, 7) == 2);
	}

	TEST_CASE("A bias moves every start")
	{
		const std::vector<float> starts{ 10.0f, 30.0f };

		CHECK(LodSelection::SelectLevel(15.0f, starts, 2.0f, 0) == 0);
		CHECK(LodSelection::SelectLevel(25.0f, starts, 2.0f, 0) == 1);
		CHECK(LodSelection::SelectLevel(70.0f, starts, 2.0f, 0) == 2);
		CHECK(LodSelection::SelectLevel(6.0f, starts, 0.5f, 0) == 1);

		// A bias that is not above 0 counts as 1
		CHECK(LodSelection::SelectLevel(15.0f, starts, 0.0f, 0) == 1);
		CHECK(LodSelection::SelectLevel(15.0f, starts, -3.0f, 0) == 1);
	}

	TEST_CASE("The distance is measured to the centre of the bounds in the world, not to the pivot")
	{
		using OvMaths::FMatrix4;

		// The geometry stands 100 m from the pivot: the pivot's distance would be 100
		const OvRendering::Geometry::BoundingSphere bounds{ { -100.0f, 0.0f, 0.0f }, 1.0f };
		const auto moved = FMatrix4::Translation({ 100.0f, 0.0f, 0.0f });
		CHECK(LodSelection::DistanceToBounds({ 0.0f, 0.0f, 0.0f }, moved, bounds) == doctest::Approx(0.0f));
		CHECK(LodSelection::DistanceToBounds({ 3.0f, 4.0f, 0.0f }, moved, bounds) == doctest::Approx(5.0f));

		// Scaled, then turned half a turn about Y
		const OvRendering::Geometry::BoundingSphere offset{ { 5.0f, 0.0f, 0.0f }, 1.0f };
		const auto scaled = FMatrix4::Translation({ 0.0f, 2.0f, 0.0f }) * FMatrix4::Scaling({ 2.0f, 2.0f, 2.0f });
		CHECK(LodSelection::DistanceToBounds({ 10.0f, 2.0f, 0.0f }, scaled, offset) == doctest::Approx(0.0f));

		const auto turned = FMatrix4::RotationOnAxisY(std::numbers::pi_v<float>);
		CHECK(LodSelection::DistanceToBounds({ -5.0f, 0.0f, 0.0f }, turned, offset) == doctest::Approx(0.0f).epsilon(1e-4));
		CHECK(LodSelection::DistanceToBounds({ 5.0f, 0.0f, 0.0f }, turned, offset) == doctest::Approx(10.0f));
	}

	TEST_CASE("A model with bones takes levels with the same bones, in the same order")
	{
		const auto body = MakeSkeleton({ "hips", "spine", "head" });

		// A model without bones takes any level
		CHECK_FALSE(LodSelection::CompareBones(std::nullopt, std::nullopt));
		CHECK_FALSE(LodSelection::CompareBones(std::nullopt, body));

		CHECK_FALSE(LodSelection::CompareBones(body, MakeSkeleton({ "hips", "spine", "head" })));

		const auto none = LodSelection::CompareBones(body, std::nullopt);
		REQUIRE(none);
		CHECK(none->find("no bones") != std::string::npos);

		const auto fewer = LodSelection::CompareBones(body, MakeSkeleton({ "hips", "spine" }));
		REQUIRE(fewer);
		CHECK(fewer->find("2 bones") != std::string::npos);

		const auto other = LodSelection::CompareBones(body, MakeSkeleton({ "hips", "head", "spine" }));
		REQUIRE(other);
		CHECK(other->find("bone 1 is 'head'") != std::string::npos);
	}

	TEST_CASE("A renderer keeps its levels through a file, and says which it leaves out")
	{
		using namespace OvCore::ECS::Components;

		OvCore::SceneSystem::Scene scene("", "");
		auto& actor = scene.CreateActor("Rock");
		auto& renderer = actor.AddComponent<CModelRenderer>();

		CHECK(renderer.GetLods().empty());
		CHECK_FALSE(renderer.GetLodError());
		CHECK(renderer.GetLodBias() == doctest::Approx(1.0f));

		// Levels with no model are kept as set, and left out of the drawing
		const std::vector<CModelRenderer::LodLevel> levels{ { nullptr, 20.0f }, { nullptr, 50.0f } };
		renderer.SetLods(levels);
		REQUIRE(renderer.GetLods().size() == 2);
		const auto error = renderer.GetLodError();
		REQUIRE(error);
		CHECK(error->find("level 1 is left out: it has no model") != std::string::npos);
		CHECK(error->find("level 2 is left out") != std::string::npos);

		// With no model, nothing to draw, and the level stays 0
		CHECK(renderer.SelectLod(OvMaths::FVector3{ 0.0f, 0.0f, 1000.0f }, true) == nullptr);
		CHECK(renderer.GetLodLevel() == 0);

		// No more than 7 levels besides the model
		renderer.SetLods(std::vector<CModelRenderer::LodLevel>(10, { nullptr, 1.0f }));
		CHECK(renderer.GetLods().size() == CModelRenderer::kMaxLodLevels);

		renderer.SetLods(levels);
		renderer.SetLodBias(1.5f);
		renderer.SetLodBias(-1.0f);
		CHECK(renderer.GetLodBias() > 0.0f);
		renderer.SetLodBias(1.5f);

		tinyxml2::XMLDocument document;
		auto* node = document.NewElement("component");
		document.InsertFirstChild(node);
		renderer.OnSerialize(document, node);

		auto& other = scene.CreateActor("Other").AddComponent<CModelRenderer>();
		other.OnDeserialize(document, node);
		const auto read = other.GetLods();
		REQUIRE(read.size() == 2);
		CHECK(read[0].distance == doctest::Approx(20.0f));
		CHECK(read[1].distance == doctest::Approx(50.0f));
		CHECK(read[0].model == nullptr);
		CHECK(other.GetLodBias() == doctest::Approx(1.5f));

		// A file from before the levels has none
		tinyxml2::XMLDocument older;
		auto* olderNode = older.NewElement("component");
		older.InsertFirstChild(olderNode);
		auto& third = scene.CreateActor("Third").AddComponent<CModelRenderer>();
		third.OnDeserialize(older, olderNode);
		CHECK(third.GetLods().empty());
		CHECK(third.GetLodBias() == doctest::Approx(1.0f));
	}
}
