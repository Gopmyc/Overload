/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace OvCore::Terrain
{
	/**
	* The largest number of samples per side a height field takes
	*/
	constexpr uint32_t kMaxHeightSamples = 4096;

	/**
	* The smallest number of samples per side a height field takes
	*/
	constexpr uint32_t kMinHeightSamples = 3;

	/**
	* A square grid of values read from an image, from 0 (black) to 1 (white), row-major: along +X (the image's
	* columns) first, then +Z (its rows, from the top)
	*/
	struct HeightmapImage
	{
		uint32_t size = 0;
		std::vector<float> values;
	};

	/**
	* Reads a height map from an encoded image (a PNG of 8 or 16 bits, or any format stb_image reads): its grey level,
	* the luminance of a colour image. Returns why it cannot (not an image, not square, or not kMinHeightSamples to
	* kMaxHeightSamples pixels per side), or nothing
	* @param p_data
	* @param p_size
	* @param p_image
	*/
	std::optional<std::string> DecodeHeightmap(const uint8_t* p_data, size_t p_size, HeightmapImage& p_image);

	/**
	* Reads a height map from an image file, as DecodeHeightmap does
	* @param p_file
	* @param p_image
	*/
	std::optional<std::string> LoadHeightmap(const std::filesystem::path& p_file, HeightmapImage& p_image);

	/**
	* The height and the slope of a height field at a point, in the field's own space: the sample (x, z) stands at
	* (x * spacing, height, z * spacing), and each cell is split along its (x, z)-(x + 1, z + 1) diagonal, as the
	* physics does
	*/
	struct HeightSample
	{
		float height = 0.0f;
		float slopeX = 0.0f; // Rise per unit along +X
		float slopeZ = 0.0f; // Rise per unit along +Z
	};

	/**
	* Returns the height and slope of a field at a point of its own space (x and z from its first sample), on the
	* triangle under it, or nothing outside the field
	* @param p_heights (row-major, along +X then +Z)
	* @param p_sampleCount (samples per side)
	* @param p_spacing
	* @param p_x
	* @param p_z
	*/
	std::optional<HeightSample> SampleHeightField(const std::vector<float>& p_heights, uint32_t p_sampleCount, float p_spacing, float p_x, float p_z);
}
