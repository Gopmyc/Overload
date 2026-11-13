/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cmath>
#include <format>
#include <fstream>
#include <iterator>
#include <limits>

#include <stb_image/stb_image.h>

#include <OvCore/Terrain/Heightmap.h>

std::optional<std::string> OvCore::Terrain::DecodeHeightmap(const uint8_t* p_data, size_t p_size, HeightmapImage& p_image)
{
	if (!p_data || p_size == 0 || p_size > static_cast<size_t>(std::numeric_limits<int>::max()))
	{
		return std::string{ "not an image" };
	}

	const int size = static_cast<int>(p_size);
	int width = 0;
	int height = 0;
	int channels = 0;

	if (!stbi_info_from_memory(p_data, size, &width, &height, &channels))
	{
		return std::format("not an image ({})", stbi_failure_reason() ? stbi_failure_reason() : "unknown format");
	}

	if (width != height)
	{
		return std::format("the image is {} x {} pixels: a height map is square", width, height);
	}

	if (static_cast<uint32_t>(width) < kMinHeightSamples || static_cast<uint32_t>(width) > kMaxHeightSamples)
	{
		return std::format("the image is {} pixels wide: a height map is {} to {} pixels wide", width, kMinHeightSamples, kMaxHeightSamples);
	}

	// The texture loader flips images for the GPU and sets this state each time: a height map keeps its rows from the
	// top, as an image viewer shows them
	stbi_set_flip_vertically_on_load(0);

	const auto count = static_cast<size_t>(width) * static_cast<size_t>(height);
	p_image.size = static_cast<uint32_t>(width);
	p_image.values.assign(count, 0.0f);

	if (stbi_is_16_bit_from_memory(p_data, size))
	{
		stbi_us* pixels = stbi_load_16_from_memory(p_data, size, &width, &height, &channels, 1);
		if (!pixels)
		{
			return std::format("cannot read the image ({})", stbi_failure_reason() ? stbi_failure_reason() : "unknown reason");
		}

		std::transform(pixels, pixels + count, p_image.values.begin(), [](stbi_us p_value) { return static_cast<float>(p_value) / 65535.0f; });
		stbi_image_free(pixels);
	}
	else
	{
		stbi_uc* pixels = stbi_load_from_memory(p_data, size, &width, &height, &channels, 1);
		if (!pixels)
		{
			return std::format("cannot read the image ({})", stbi_failure_reason() ? stbi_failure_reason() : "unknown reason");
		}

		std::transform(pixels, pixels + count, p_image.values.begin(), [](stbi_uc p_value) { return static_cast<float>(p_value) / 255.0f; });
		stbi_image_free(pixels);
	}

	return std::nullopt;
}

std::optional<std::string> OvCore::Terrain::LoadHeightmap(const std::filesystem::path& p_file, HeightmapImage& p_image)
{
	std::ifstream file(p_file, std::ios::binary);
	if (!file)
	{
		return std::format("cannot read '{}'", p_file.generic_string());
	}

	const std::vector<uint8_t> bytes{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
	return DecodeHeightmap(bytes.data(), bytes.size(), p_image);
}

std::optional<OvCore::Terrain::HeightSample> OvCore::Terrain::SampleHeightField(const std::vector<float>& p_heights, uint32_t p_sampleCount, float p_spacing, float p_x, float p_z)
{
	if (p_sampleCount < 2 || p_spacing <= 0.0f || p_heights.size() != static_cast<size_t>(p_sampleCount) * p_sampleCount)
	{
		return std::nullopt;
	}

	const float gridX = p_x / p_spacing;
	const float gridZ = p_z / p_spacing;
	const float last = static_cast<float>(p_sampleCount - 1);

	if (!(gridX >= 0.0f && gridZ >= 0.0f && gridX <= last && gridZ <= last))
	{
		return std::nullopt;
	}

	// The cell under the point; a point on the far edge belongs to the last cell
	const uint32_t cellX = std::min(static_cast<uint32_t>(gridX), p_sampleCount - 2);
	const uint32_t cellZ = std::min(static_cast<uint32_t>(gridZ), p_sampleCount - 2);
	const float fx = gridX - static_cast<float>(cellX);
	const float fz = gridZ - static_cast<float>(cellZ);

	const auto at = [&](uint32_t p_sx, uint32_t p_sz) { return p_heights[static_cast<size_t>(p_sz) * p_sampleCount + p_sx]; };
	const float h00 = at(cellX, cellZ);
	const float h10 = at(cellX + 1, cellZ);
	const float h01 = at(cellX, cellZ + 1);
	const float h11 = at(cellX + 1, cellZ + 1);

	// Two triangles share the (0, 0)-(1, 1) diagonal: (0, 0), (1, 0), (1, 1) where fx >= fz, else (0, 0), (0, 1), (1, 1)
	HeightSample sample;

	if (fx >= fz)
	{
		sample.height = h00 + fx * (h10 - h00) + fz * (h11 - h10);
		sample.slopeX = (h10 - h00) / p_spacing;
		sample.slopeZ = (h11 - h10) / p_spacing;
	}
	else
	{
		sample.height = h00 + fz * (h01 - h00) + fx * (h11 - h01);
		sample.slopeX = (h11 - h01) / p_spacing;
		sample.slopeZ = (h01 - h00) / p_spacing;
	}

	return sample;
}
