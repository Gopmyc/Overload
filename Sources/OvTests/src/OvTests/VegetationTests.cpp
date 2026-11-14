/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cmath>
#include <numbers>
#include <tuple>
#include <vector>

#include <doctest/doctest.h>
#include <tinyxml2.h>

#include <OvCore/ECS/Actor.h>
#include <OvCore/ECS/Components/CPhysicalHeightField.h>
#include <OvCore/ECS/Components/CVegetation.h>
#include <OvCore/SceneSystem/Scene.h>
#include <OvCore/Terrain/Scatter.h>
#include <OvPhysics/Core/PhysicsEngine.h>

using namespace OvCore::Terrain;

namespace
{
	std::vector<float> Flat(uint32_t p_count, float p_height = 0.0f)
	{
		return std::vector<float>(static_cast<size_t>(p_count) * p_count, p_height);
	}

	// Rising along +X at an angle
	std::vector<float> Slope(uint32_t p_count, float p_spacing, float p_degrees)
	{
		std::vector<float> heights(static_cast<size_t>(p_count) * p_count);
		const float rise = std::tan(p_degrees * std::numbers::pi_v<float> / 180.0f) * p_spacing;
		for (uint32_t z = 0; z < p_count; ++z)
		{
			for (uint32_t x = 0; x < p_count; ++x)
			{
				heights[z * p_count + x] = static_cast<float>(x) * rise;
			}
		}
		return heights;
	}

	std::vector<ScatterInstance> ScatterAll(const std::vector<float>& p_heights, const TerrainLayout& p_layout, const ScatterSettings& p_settings, const std::vector<float>& p_map = {}, uint32_t p_mapSize = 0, bool p_reversed = false)
	{
		std::vector<ScatterInstance> all;
		std::vector<ScatterInstance> chunk;
		const uint32_t count = p_layout.GetChunkCount();

		for (uint32_t i = 0; i < count; ++i)
		{
			ScatterChunk(p_heights, p_layout, p_reversed ? count - 1 - i : i, p_settings, p_map, p_mapSize, chunk);
			all.insert(all.end(), chunk.begin(), chunk.end());
		}

		return all;
	}

	// The instances in an order of their own, to compare scatters made in different orders
	std::vector<std::tuple<float, float, float>> Positions(const std::vector<ScatterInstance>& p_instances)
	{
		std::vector<std::tuple<float, float, float>> positions;
		for (const auto& instance : p_instances)
		{
			positions.emplace_back(instance.position.x, instance.position.z, instance.position.y);
		}
		std::sort(positions.begin(), positions.end());
		return positions;
	}
}

TEST_SUITE("Vegetation")
{
	TEST_CASE("A field scatters the same for a seed, whatever the order and the size of its chunks")
	{
		const auto heights = Flat(65);
		ScatterSettings settings;
		settings.density = 0.5f;
		settings.seed = 42;

		const auto small = TerrainLayout::Make(65, 1.0f, 16, 1);
		const auto whole = TerrainLayout::Make(65, 1.0f, 64, 1);
		REQUIRE(small.GetChunkCount() == 16);
		REQUIRE(whole.GetChunkCount() == 1);

		const auto inOrder = ScatterAll(heights, small, settings);
		const auto reversed = ScatterAll(heights, small, settings, {}, 0, true);
		const auto atOnce = ScatterAll(heights, whole, settings);

		REQUIRE(!inOrder.empty());
		CHECK(Positions(inOrder) == Positions(reversed));
		CHECK(Positions(inOrder) == Positions(atOnce)); // No cell in two chunks, none missed

		// Another seed, another scatter
		settings.seed = 43;
		CHECK(Positions(ScatterAll(heights, small, settings)) != Positions(inOrder));

		// Every instance stands in its chunk
		std::vector<ScatterInstance> chunk;
		for (uint32_t i = 0; i < small.GetChunkCount(); ++i)
		{
			uint32_t x0 = 0, x1 = 0, z0 = 0, z1 = 0;
			small.GetChunkSamples(i, x0, x1, z0, z1);
			ScatterChunk(heights, small, i, settings, {}, 0, chunk);
			for (const auto& instance : chunk)
			{
				CHECK(instance.position.x >= static_cast<float>(x0) - 1.5f);
				CHECK(instance.position.x <= static_cast<float>(x1) + 1.5f);
				CHECK(instance.position.z >= static_cast<float>(z0) - 1.5f);
				CHECK(instance.position.z <= static_cast<float>(z1) + 1.5f);
			}
		}
	}

	TEST_CASE("The density holds over the field, and a density map weighs each place")
	{
		const auto heights = Flat(129, 3.0f);
		const auto layout = TerrainLayout::Make(129, 1.0f, 32, 1);
		ScatterSettings settings;
		settings.density = 2.0f;

		// 128 m by 128 m at 2 per square metre
		const auto all = ScatterAll(heights, layout, settings);
		CHECK(static_cast<double>(all.size()) == doctest::Approx(128.0 * 128.0 * 2.0).epsilon(0.01));
		CHECK(std::all_of(all.begin(), all.end(), [](const ScatterInstance& p_instance) { return p_instance.position.y == doctest::Approx(3.0f); }));

		// A map of zeros empties the field, a map of ones changes nothing
		CHECK(ScatterAll(heights, layout, settings, { 0.0f, 0.0f, 0.0f, 0.0f }, 2).empty());
		CHECK(Positions(ScatterAll(heights, layout, settings, { 1.0f, 1.0f, 1.0f, 1.0f }, 2)) == Positions(all));

		// A map laid like the heights, zero on the half where x < 64: nothing grows there
		std::vector<float> half(129 * 129);
		for (uint32_t z = 0; z < 129; ++z)
		{
			for (uint32_t x = 0; x < 129; ++x)
			{
				half[z * 129 + x] = x < 64 ? 0.0f : 1.0f;
			}
		}

		const auto halved = ScatterAll(heights, layout, settings, half, 129);
		CHECK(std::all_of(halved.begin(), halved.end(), [](const ScatterInstance& p_instance) { return p_instance.position.x > 63.0f; }));
		CHECK(static_cast<double>(halved.size()) == doctest::Approx(all.size() / 2.0).epsilon(0.03));

		// Weights fading along X keep a share of the instances in between
		const auto fading = ScatterAll(heights, layout, settings, { 0.0f, 1.0f, 0.0f, 1.0f }, 2);
		CHECK(static_cast<double>(fading.size()) == doctest::Approx(all.size() / 2.0).epsilon(0.05));

		CHECK(SampleDensity({}, 0, 0.3f, 0.7f) == doctest::Approx(1.0f));
		CHECK(SampleDensity({ 0.0f, 1.0f, 0.0f, 1.0f }, 2, 0.25f, 0.5f) == doctest::Approx(0.25f));
	}

	TEST_CASE("Nothing stands on too steep a slope, or out of the heights given")
	{
		const auto ramp = Slope(65, 1.0f, 30.0f);
		const auto layout = TerrainLayout::Make(65, 1.0f, 32, 1);
		ScatterSettings settings;
		settings.density = 1.0f;

		settings.maxSlope = 25.0f;
		CHECK(ScatterAll(ramp, layout, settings).empty());

		settings.maxSlope = 35.0f;
		const auto all = ScatterAll(ramp, layout, settings);
		REQUIRE(!all.empty());

		// The ramp rises to 64 × tan 30° = 36.9 m: half its height keeps the first half of it
		settings.maxHeight = 18.5f;
		const auto low = ScatterAll(ramp, layout, settings);
		CHECK(std::all_of(low.begin(), low.end(), [](const ScatterInstance& p_instance) { return p_instance.position.y <= 18.5f; }));
		CHECK(static_cast<double>(low.size()) == doctest::Approx(all.size() / 2.0).epsilon(0.05));

		settings.maxHeight = 10000.0f;
		settings.minHeight = 18.5f;
		const auto high = ScatterAll(ramp, layout, settings);
		CHECK(low.size() + high.size() == all.size());
	}

	TEST_CASE("An instance stands upright or along the ground, turned and sized as asked")
	{
		const auto ramp = Slope(33, 1.0f, 30.0f);
		const auto layout = TerrainLayout::Make(33, 1.0f, 32, 1);
		const OvMaths::FVector3 up{ 0.0f, 1.0f, 0.0f };
		const auto normal = OvMaths::FVector3::Normalize({ -std::tan(30.0f * std::numbers::pi_v<float> / 180.0f), 1.0f, 0.0f });

		ScatterSettings settings;
		settings.density = 0.5f;
		settings.minScale = 0.5f;
		settings.maxScale = 2.0f;

		std::vector<ScatterInstance> instances;

		// Upright, turned about the vertical only
		ScatterChunk(ramp, layout, 0, settings, {}, 0, instances);
		REQUIRE(!instances.empty());
		bool turned = false;
		for (const auto& instance : instances)
		{
			const auto pointing = OvMaths::FQuaternion::RotatePoint(up, instance.rotation);
			CHECK(pointing.y == doctest::Approx(1.0f).epsilon(1e-4));
			CHECK(instance.scale >= 0.5f);
			CHECK(instance.scale <= 2.0f);
			turned = turned || std::abs(instance.rotation.y) > 0.1f;
		}
		CHECK(turned);

		// Along the ground
		settings.slopeAlignment = 1.0f;
		ScatterChunk(ramp, layout, 0, settings, {}, 0, instances);
		for (const auto& instance : instances)
		{
			const auto pointing = OvMaths::FQuaternion::RotatePoint(up, instance.rotation);
			CHECK(OvMaths::FVector3::Dot(pointing, normal) == doctest::Approx(1.0f).epsilon(1e-4));
		}

		// Halfway, with no turn
		settings.slopeAlignment = 0.5f;
		settings.randomYaw = false;
		ScatterChunk(ramp, layout, 0, settings, {}, 0, instances);
		for (const auto& instance : instances)
		{
			const auto pointing = OvMaths::FQuaternion::RotatePoint(up, instance.rotation);
			CHECK(std::acos(std::clamp(pointing.y, -1.0f, 1.0f)) * 180.0f / std::numbers::pi_v<float> == doctest::Approx(15.0f).epsilon(1e-3));
			CHECK(pointing.z == doctest::Approx(0.0f).epsilon(1e-4));
		}
	}

	TEST_CASE("The bounds of a chunk hold every instance's model")
	{
		const auto ramp = Slope(65, 1.0f, 20.0f);
		const auto layout = TerrainLayout::Make(65, 1.0f, 32, 1);
		ScatterSettings settings;
		settings.density = 0.3f;
		settings.maxScale = 3.0f;

		const OvRendering::Geometry::BoundingSphere model{ { 0.0f, 0.5f, 0.0f }, 0.75f };
		std::vector<ScatterInstance> instances;

		for (uint32_t chunk = 0; chunk < layout.GetChunkCount(); ++chunk)
		{
			ScatterChunk(ramp, layout, chunk, settings, {}, 0, instances);
			const auto bounds = BoundInstances(instances, model);

			for (const auto& instance : instances)
			{
				const float reach = (0.5f + 0.75f) * instance.scale;
				CHECK(OvMaths::FVector3::Distance(bounds.position, instance.position) + reach <= bounds.radius + 1e-3f);
			}
		}

		CHECK(BoundInstances({}, model).radius == 0.0f);
	}

	TEST_CASE("A vegetation says what keeps it from being drawn, and keeps its layers through a file")
	{
		using namespace OvCore::ECS::Components;

		OvPhysics::Core::PhysicsEngine engine{ {} };
		OvCore::SceneSystem::Scene scene("", "");
		auto& actor = scene.CreateActor("Meadow");
		auto& vegetation = actor.AddComponent<CVegetation>();

		REQUIRE(vegetation.GetError());
		CHECK(vegetation.GetError()->find("no Physical Height Field") != std::string::npos);

		auto& field = actor.AddComponent<CPhysicalHeightField>();
		REQUIRE(field.SetHeights(33, 1.0f, Flat(33)));
		CHECK_FALSE(vegetation.GetError());

		// A layer without a model cannot be drawn, and says so
		CVegetation::Layer grass;
		grass.name = "Grass";
		grass.scatter.density = 3.0f;
		grass.scatter.seed = 9;
		grass.drawDistance = 25.0f;
		grass.castShadows = true;
		grass.densityMap = "Textures/Meadow.png";
		REQUIRE(vegetation.AddLayer(grass) == 0u);
		REQUIRE(vegetation.GetError());
		CHECK(vegetation.GetError()->find("layer 0 ('Grass')") != std::string::npos);

		std::vector<CVegetation::LayerDraw> draws;
		vegetation.PrepareDraw(OvMaths::FVector3{ 16.0f, 2.0f, 16.0f }, true, draws);
		CHECK(draws.empty());
		CHECK(vegetation.GetInstanceCount() == 0);

		// Density maps a script gives
		const std::vector<float> nine(9, 0.5f);
		CHECK_FALSE(vegetation.SetDensityMap(0, 3, nine));
		CHECK(vegetation.GetLayer(0)->densityMap.empty()); // The weights take the place of the image
		CHECK(vegetation.SetDensityMap(0, 3, std::vector<float>(4, 0.5f)));
		CHECK(vegetation.SetDensityMap(0, 1, std::vector<float>(1, 0.5f)));
		CHECK(vegetation.SetDensityMap(4, 3, nine));
		CHECK_FALSE(vegetation.SetDensityMap(0, 0, {}));

		// Up to 8 layers; removing one moves the next ones down
		CVegetation::Layer stones;
		stones.name = "Stones";
		REQUIRE(vegetation.AddLayer(stones) == 1u);
		for (uint32_t i = 2; i < CVegetation::kMaxLayers; ++i)
		{
			CHECK(vegetation.AddLayer(stones));
		}
		CHECK_FALSE(vegetation.AddLayer(stones));
		vegetation.RemoveLayer(0);
		CHECK(vegetation.GetLayer(0)->name == "Stones");
		while (vegetation.GetLayerCount() > 1)
		{
			vegetation.RemoveLayer(vegetation.GetLayerCount() - 1);
		}

		grass.densityMap = "Textures/Meadow.png";
		vegetation.SetLayer(0, grass);
		REQUIRE(vegetation.AddLayer(stones) == 1u);

		tinyxml2::XMLDocument document;
		auto* node = document.NewElement("component");
		document.InsertFirstChild(node);
		vegetation.OnSerialize(document, node);

		auto& other = scene.CreateActor("Other").AddComponent<CVegetation>();
		other.OnDeserialize(document, node);
		REQUIRE(other.GetLayerCount() == 2);
		const auto read = other.GetLayer(0).value();
		CHECK(read.name == "Grass");
		CHECK(read.scatter.density == doctest::Approx(3.0f));
		CHECK(read.scatter.seed == 9);
		CHECK(read.drawDistance == doctest::Approx(25.0f));
		CHECK(read.castShadows);
		CHECK(read.densityMap == "Textures/Meadow.png");
		CHECK(read.model == nullptr);
		CHECK(other.GetLayer(1)->name == "Stones");

		// A file from before any layer
		tinyxml2::XMLDocument empty;
		auto* emptyNode = empty.NewElement("component");
		empty.InsertFirstChild(emptyNode);
		other.OnDeserialize(empty, emptyNode);
		CHECK(other.GetLayerCount() == 0);
	}
}
