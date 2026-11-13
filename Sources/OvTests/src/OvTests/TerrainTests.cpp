/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <tinyxml2.h>

#include <OvCore/ECS/Actor.h>
#include <OvCore/ECS/Components/CPhysicalHeightField.h>
#include <OvCore/ECS/Components/CTerrain.h>
#include <OvCore/SceneSystem/Scene.h>
#include <OvCore/Terrain/Heightmap.h>
#include <OvCore/Terrain/TerrainMesher.h>
#include <OvPhysics/Core/PhysicsEngine.h>

using namespace OvCore::Terrain;

namespace
{
	// A grey PNG of 8 or 16 bits, its deflate blocks stored: enough to feed the decoder without a codec
	std::vector<uint8_t> EncodeGreyPng(uint32_t p_width, uint32_t p_height, const std::vector<uint16_t>& p_values, bool p_sixteenBits)
	{
		const auto crc = [](const uint8_t* p_data, size_t p_size, uint32_t p_crc = 0xFFFFFFFFu) {
			for (size_t i = 0; i < p_size; ++i)
			{
				p_crc ^= p_data[i];
				for (int bit = 0; bit < 8; ++bit)
				{
					p_crc = (p_crc >> 1) ^ (0xEDB88320u & (0u - (p_crc & 1u)));
				}
			}
			return p_crc;
		};

		std::vector<uint8_t> png{ 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
		const auto put32 = [](std::vector<uint8_t>& p_out, uint32_t p_value) {
			p_out.insert(p_out.end(), { uint8_t(p_value >> 24), uint8_t(p_value >> 16), uint8_t(p_value >> 8), uint8_t(p_value) });
		};
		const auto chunk = [&](const char* p_type, const std::vector<uint8_t>& p_data) {
			put32(png, static_cast<uint32_t>(p_data.size()));
			std::vector<uint8_t> body(p_type, p_type + 4);
			body.insert(body.end(), p_data.begin(), p_data.end());
			png.insert(png.end(), body.begin(), body.end());
			put32(png, crc(body.data(), body.size()) ^ 0xFFFFFFFFu);
		};

		std::vector<uint8_t> header;
		put32(header, p_width);
		put32(header, p_height);
		header.insert(header.end(), { uint8_t(p_sixteenBits ? 16 : 8), 0, 0, 0, 0 });
		chunk("IHDR", header);

		std::vector<uint8_t> raw;
		for (uint32_t y = 0; y < p_height; ++y)
		{
			raw.push_back(0);
			for (uint32_t x = 0; x < p_width; ++x)
			{
				const uint16_t value = p_values[y * p_width + x];
				if (p_sixteenBits)
				{
					raw.push_back(uint8_t(value >> 8));
				}
				raw.push_back(uint8_t(value));
			}
		}

		std::vector<uint8_t> zlib{ 0x78, 0x01 };
		for (size_t offset = 0; offset < raw.size(); offset += 65535)
		{
			const auto length = static_cast<uint16_t>(std::min<size_t>(65535, raw.size() - offset));
			zlib.push_back(offset + length >= raw.size() ? 1 : 0);
			zlib.insert(zlib.end(), { uint8_t(length), uint8_t(length >> 8), uint8_t(~length), uint8_t(uint16_t(~length) >> 8) });
			zlib.insert(zlib.end(), raw.begin() + static_cast<std::ptrdiff_t>(offset), raw.begin() + static_cast<std::ptrdiff_t>(offset + length));
		}
		uint32_t a = 1, b = 0;
		for (const uint8_t byte : raw)
		{
			a = (a + byte) % 65521;
			b = (b + a) % 65521;
		}
		put32(zlib, (b << 16) | a);
		chunk("IDAT", zlib);
		chunk("IEND", {});
		return png;
	}

	std::filesystem::path WriteTemporary(const std::string& p_name, const std::vector<uint8_t>& p_bytes)
	{
		const auto path = std::filesystem::temp_directory_path() / ("ovtests_terrain_" + p_name);
		std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(p_bytes.data()), static_cast<std::streamsize>(p_bytes.size()));
		return path;
	}

	std::vector<float> Ramp(uint32_t p_count)
	{
		std::vector<float> heights(static_cast<size_t>(p_count) * p_count);
		for (uint32_t z = 0; z < p_count; ++z)
		{
			for (uint32_t x = 0; x < p_count; ++x)
			{
				// Bumpy enough that the levels of detail differ
				heights[z * p_count + x] = std::sin(static_cast<float>(x) * 0.7f) * 2.0f + std::cos(static_cast<float>(z) * 0.9f) * 1.5f + static_cast<float>(x + z) * 0.1f;
			}
		}
		return heights;
	}
}

TEST_SUITE("Terrain")
{
	TEST_CASE("A height map is read from a grey image of 8 or 16 bits, square, its rows from the top")
	{
		HeightmapImage image;

		// 8 bits: a black top left, a white bottom right
		const std::vector<uint16_t> eight{ 0, 51, 102, 153, 204, 255, 0, 0, 255 };
		REQUIRE_FALSE(DecodeHeightmap(EncodeGreyPng(3, 3, eight, false).data(), EncodeGreyPng(3, 3, eight, false).size(), image));
		CHECK(image.size == 3);
		REQUIRE(image.values.size() == 9);
		CHECK(image.values[0] == doctest::Approx(0.0f));
		CHECK(image.values[1] == doctest::Approx(0.2f));
		CHECK(image.values[5] == doctest::Approx(1.0f));
		CHECK(image.values[8] == doctest::Approx(1.0f));

		// 16 bits keep the steps 8 bits lose
		const std::vector<uint16_t> sixteen{ 0, 1, 2, 3, 4, 5, 6, 7, 65535 };
		const auto bytes = EncodeGreyPng(3, 3, sixteen, true);
		REQUIRE_FALSE(DecodeHeightmap(bytes.data(), bytes.size(), image));
		CHECK(image.size == 3);
		CHECK(image.values[1] == doctest::Approx(1.0f / 65535.0f).epsilon(1e-6));
		CHECK(image.values[2] == doctest::Approx(2.0f / 65535.0f).epsilon(1e-6));
		CHECK(image.values[8] == doctest::Approx(1.0f));
	}

	TEST_CASE("A height map that is not square, too small or not an image is refused, and says why")
	{
		HeightmapImage image;

		const auto wide = EncodeGreyPng(3, 2, { 0, 0, 0, 0, 0, 0 }, false);
		const auto wideError = DecodeHeightmap(wide.data(), wide.size(), image);
		REQUIRE(wideError);
		CHECK(wideError->find("3 x 2") != std::string::npos);

		const auto tiny = EncodeGreyPng(2, 2, { 0, 0, 0, 0 }, false);
		const auto tinyError = DecodeHeightmap(tiny.data(), tiny.size(), image);
		REQUIRE(tinyError);
		CHECK(tinyError->find("3 to 4096") != std::string::npos);

		const std::vector<uint8_t> text{ 'n', 'o', 't', ' ', 'a', 'n', ' ', 'i', 'm', 'a', 'g', 'e' };
		CHECK(DecodeHeightmap(text.data(), text.size(), image));

		const auto missing = LoadHeightmap(std::filesystem::temp_directory_path() / "ovtests_terrain_nowhere.png", image);
		REQUIRE(missing);
		CHECK(missing->find("cannot read") != std::string::npos);
	}

	TEST_CASE("The height under a point follows the triangles of the physics, split along each cell's diagonal")
	{
		// One cell, 2 apart: (0, 0) = 0, (1, 0) = 4, (0, 1) = 2, (1, 1) = 8
		const std::vector<float> heights{ 0.0f, 4.0f, 2.0f, 8.0f };

		const auto corner = SampleHeightField(heights, 2, 2.0f, 2.0f, 2.0f);
		REQUIRE(corner);
		CHECK(corner->height == doctest::Approx(8.0f));

		// On the diagonal, both triangles agree: halfway between 0 and 8
		CHECK(SampleHeightField(heights, 2, 2.0f, 1.0f, 1.0f)->height == doctest::Approx(4.0f));

		// Below the diagonal (x > z): the triangle (0, 0), (1, 0), (1, 1)
		const auto below = SampleHeightField(heights, 2, 2.0f, 1.5f, 0.5f);
		REQUIRE(below);
		CHECK(below->height == doctest::Approx(0.75f * 4.0f + 0.25f * 4.0f));
		CHECK(below->slopeX == doctest::Approx(2.0f));
		CHECK(below->slopeZ == doctest::Approx(2.0f));

		// Above it (z > x): the triangle (0, 0), (0, 1), (1, 1)
		const auto above = SampleHeightField(heights, 2, 2.0f, 0.5f, 1.5f);
		REQUIRE(above);
		CHECK(above->height == doctest::Approx(0.75f * 2.0f + 0.25f * 6.0f));
		CHECK(above->slopeX == doctest::Approx(3.0f));
		CHECK(above->slopeZ == doctest::Approx(1.0f));

		CHECK_FALSE(SampleHeightField(heights, 2, 2.0f, -0.1f, 1.0f));
		CHECK_FALSE(SampleHeightField(heights, 2, 2.0f, 1.0f, 2.1f));
		CHECK_FALSE(SampleHeightField({}, 2, 2.0f, 1.0f, 1.0f));
	}

	TEST_CASE("A height field is made from a height map, read again when its scale changes, and left by SetHeights")
	{
		using namespace OvCore::ECS::Components;

		OvPhysics::Core::PhysicsEngine engine{ {} };
		OvCore::SceneSystem::Scene scene("", "");

		const auto path = WriteTemporary("map.png", EncodeGreyPng(3, 3, { 0, 0, 0, 0, 255, 0, 0, 0, 0 }, false));

		auto& actor = scene.CreateActor("Field");
		actor.transform.SetLocalPosition({ 10.0f, 1.0f, -4.0f });
		auto& field = actor.AddComponent<CPhysicalHeightField>();
		field.SetSpacing(2.0f);
		field.SetHeightOffset(-1.0f);
		field.SetHeightScale(6.0f);

		const auto before = field.GetHeightsRevision();
		field.SetHeightmap(path.string());
		CHECK_FALSE(field.GetHeightmapError());
		CHECK(field.GetHeightsRevision() != before);
		REQUIRE(field.GetSampleCount() == 3);
		CHECK(field.GetSpacing() == doctest::Approx(2.0f));
		CHECK(field.GetHeights()[4] == doctest::Approx(5.0f));
		CHECK(field.GetHeights()[0] == doctest::Approx(-1.0f));

		// In the world: the actor raised by 1, the peak at the centre sample (2, 2) from the actor
		CHECK(field.GetHeightAt(12.0f, -2.0f).value() == doctest::Approx(6.0f));
		CHECK(field.GetHeightAt(10.0f, -4.0f).value() == doctest::Approx(0.0f));
		CHECK_FALSE(field.GetHeightAt(9.0f, -4.0f));

		const auto normal = field.GetNormalAt(11.5f, -3.5f);
		REQUIRE(normal);
		CHECK(normal->y > 0.0f);
		CHECK(OvMaths::FVector3::Length(normal.value()) == doctest::Approx(1.0f));

		// The scale reads the map again
		field.SetHeightScale(12.0f);
		CHECK(field.GetHeights()[4] == doctest::Approx(11.0f));

		// A turn about Y carries the field with it
		actor.transform.SetLocalRotation(OvMaths::FQuaternion({ 0.0f, 90.0f, 0.0f }));
		const auto turnedCentre = actor.transform.GetWorldMatrix() * OvMaths::FVector4{ 2.0f, 0.0f, 2.0f, 1.0f };
		CHECK(field.GetHeightAt(turnedCentre.x, turnedCentre.z).value() == doctest::Approx(12.0f).epsilon(1e-4));
		actor.transform.SetLocalRotation(OvMaths::FQuaternion::Identity);

		// A script's heights take the field over
		REQUIRE(field.SetHeights(3, 1.0f, std::vector<float>(9, 1.0f)));
		CHECK(field.GetHeightmap().empty());

		// A map that cannot be read empties the field, and says why
		field.SetHeightmap((std::filesystem::temp_directory_path() / "ovtests_terrain_nowhere.png").string());
		REQUIRE(field.GetHeightmapError());
		CHECK(field.GetHeightmapError()->find("cannot read") != std::string::npos);
		CHECK(field.GetHeights().empty());

		std::filesystem::remove(path);
	}

	TEST_CASE("A field is cut into chunks, a level of detail keeping one sample in 2^n and each chunk's last row")
	{
		const auto layout = TerrainLayout::Make(129, 2.0f, 16, 4);
		CHECK(layout.chunksPerSide == 8);
		CHECK(layout.GetChunkCount() == 64);
		CHECK(layout.lodCount == 4);

		// A chunk of 2 cells only halves once; a side of cells not a multiple of the chunk leaves a smaller last chunk
		CHECK(TerrainLayout::Make(129, 2.0f, 2, 4).lodCount == 2);
		const auto uneven = TerrainLayout::Make(20, 1.0f, 8, 4);
		CHECK(uneven.chunksPerSide == 3);
		uint32_t x0, x1, z0, z1;
		uneven.GetChunkSamples(8, x0, x1, z0, z1);
		CHECK(x0 == 16);
		CHECK(x1 == 19);
		CHECK(z1 == 19);

		const auto heights = Ramp(129);
		ChunkGeometry geometry;

		const std::array<uint32_t, 4> sides{ 17, 9, 5, 3 };
		for (uint32_t lod = 0; lod < 4; ++lod)
		{
			BuildChunk(heights, layout, 9, lod, 1.0f, geometry);
			const uint32_t side = sides[lod];
			CHECK(geometry.surfaceTriangles == 2 * (side - 1) * (side - 1));

			// The surface, then a copy of each edge for the skirts
			CHECK(geometry.vertices.size() == side * side + 4 * side);
			CHECK(geometry.indices.size() == 3 * geometry.surfaceTriangles + 4 * (side - 1) * 12);
		}

		// The surface sits on the heights, its texture coordinates on the sample centres, its first triangle facing up
		BuildChunk(heights, layout, 0, 0, 1.0f, geometry);
		const auto& corner = geometry.vertices[0];
		CHECK(corner.position[1] == doctest::Approx(heights[0]));
		CHECK(corner.texCoords[0] == doctest::Approx(0.5f / 129.0f));
		CHECK(corner.texCoords[1] == doctest::Approx(1.0f - 0.5f / 129.0f));

		const auto point = [&](uint32_t p_index) {
			const auto& v = geometry.vertices[geometry.indices[p_index]];
			return OvMaths::FVector3{ v.position[0], v.position[1], v.position[2] };
		};
		const auto facing = OvMaths::FVector3::Cross(point(1) - point(0), point(2) - point(0));
		CHECK(facing.y > 0.0f);
	}

	TEST_CASE("Neighbouring chunks at the same level share their edge, and the skirts cover the gap between levels")
	{
		const auto layout = TerrainLayout::Make(65, 1.0f, 16, 4);
		const auto heights = Ramp(65);
		ChunkGeometry left;
		ChunkGeometry right;

		for (uint32_t lod = 0; lod < 4; ++lod)
		{
			BuildChunk(heights, layout, 0, lod, 0.0f, left);
			BuildChunk(heights, layout, 1, lod, 0.0f, right);

			// The left chunk's last column against the right chunk's first
			const uint32_t side = (16u >> lod) + 1;
			for (uint32_t row = 0; row < side; ++row)
			{
				const auto& a = left.vertices[row * side + side - 1];
				const auto& b = right.vertices[row * side];
				CHECK(a.position[0] == doctest::Approx(b.position[0]));
				CHECK(a.position[1] == doctest::Approx(b.position[1]));
				CHECK(a.position[2] == doctest::Approx(b.position[2]));
			}
		}

		// Along a shared edge, the full detail and the coarsest level part by less than the skirt hangs
		const float depth = ComputeSkirtDepth(heights, layout, 0);
		const auto box = ComputeChunkBox(heights, layout, 0, depth);
		CHECK(depth > 0.1f);
		for (uint32_t z = 0; z <= 16; ++z)
		{
			const float fine = heights[z * 65 + 16];
			const uint32_t a = (z / 8) * 8;
			const uint32_t b = std::min(a + 8, 16u);
			const float t = b > a ? static_cast<float>(z - a) / static_cast<float>(b - a) : 0.0f;
			const float coarse = heights[a * 65 + 16] + t * (heights[b * 65 + 16] - heights[a * 65 + 16]);
			CHECK(std::abs(fine - coarse) <= depth);
			CHECK(fine - depth >= box.min.y - 1e-4f);
		}
	}

	TEST_CASE("A level is chosen by distance, doubling at each step, with a margin against flicker")
	{
		CHECK(SelectLod(10.0f, 50.0f, 4, 0) == 0);
		CHECK(SelectLod(60.0f, 50.0f, 4, 0) == 1);
		CHECK(SelectLod(150.0f, 50.0f, 4, 0) == 2);
		CHECK(SelectLod(1000.0f, 50.0f, 4, 0) == 3);
		CHECK(SelectLod(1000.0f, 50.0f, 2, 0) == 1);
		CHECK(SelectLod(1000.0f, 50.0f, 1, 0) == 0);

		// Just past a boundary, a chunk keeps its level until past the margin, either way
		CHECK(SelectLod(52.0f, 50.0f, 4, 0) == 0);
		CHECK(SelectLod(56.0f, 50.0f, 4, 0) == 1);
		CHECK(SelectLod(48.0f, 50.0f, 4, 1) == 1);
		CHECK(SelectLod(44.0f, 50.0f, 4, 1) == 0);

		CHECK(DistanceToBox({ 0.0f, 0.0f, 0.0f }, { 1.0f, -1.0f, -1.0f }, { 2.0f, 1.0f, 1.0f }) == doctest::Approx(1.0f));
		CHECK(DistanceToBox({ 1.5f, 0.0f, 0.0f }, { 1.0f, -1.0f, -1.0f }, { 2.0f, 1.0f, 1.0f }) == doctest::Approx(0.0f));
	}

	TEST_CASE("A terrain says why it draws nothing, follows its field's heights, and keeps its settings")
	{
		using namespace OvCore::ECS::Components;

		OvPhysics::Core::PhysicsEngine engine{ {} };
		OvCore::SceneSystem::Scene scene("", "");
		auto& actor = scene.CreateActor("Terrain");
		auto& terrain = actor.AddComponent<CTerrain>();

		REQUIRE(terrain.GetError());
		CHECK(terrain.GetError()->find("no Physical Height Field") != std::string::npos);
		CHECK(terrain.GetChunkCount() == 0);

		auto& field = actor.AddComponent<CPhysicalHeightField>();
		REQUIRE(terrain.GetError());
		CHECK(terrain.GetError()->find("no heights") != std::string::npos);

		REQUIRE(field.SetHeights(65, 1.0f, Ramp(65)));
		terrain.SetChunkSize(16);
		CHECK(terrain.GetChunkCount() == 16);
		CHECK(terrain.GetChunkLod(0).value() == 0);
		CHECK_FALSE(terrain.GetChunkLod(16));

		REQUIRE(field.SetHeights(33, 1.0f, Ramp(33)));
		CHECK(terrain.GetChunkCount() == 4);

		// Colours: three values per sample, or none
		CHECK_FALSE(terrain.SetColors(3, { 1.0f, 0.0f }));
		CHECK_FALSE(terrain.HasColors());
		CHECK(terrain.SetColors(2, std::vector<float>(12, 0.5f)));
		CHECK(terrain.HasColors());
		terrain.SetColorMap("Textures/Ground.png");
		CHECK_FALSE(terrain.HasColors());

		// The settings through a file
		terrain.SetLodCount(3);
		terrain.SetLodDistance(80.0f);
		terrain.SetMaterialPath("Materials/Rock.ovmat");

		tinyxml2::XMLDocument document;
		auto* node = document.NewElement("component");
		document.InsertFirstChild(node);
		terrain.OnSerialize(document, node);

		auto& other = scene.CreateActor("Other").AddComponent<CTerrain>();
		other.OnDeserialize(document, node);
		CHECK(other.GetChunkSize() == 16);
		CHECK(other.GetLodCount() == 3);
		CHECK(other.GetLodDistance() == doctest::Approx(80.0f));
		CHECK(other.GetMaterialPath() == "Materials/Rock.ovmat");
		CHECK(other.GetColorMap() == "Textures/Ground.png");
	}
}
