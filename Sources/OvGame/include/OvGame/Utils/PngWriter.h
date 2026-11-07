/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <filesystem>
#include <span>

namespace OvGame::Utils
{
	/**
	* Writes an image as a PNG file, uncompressed (stored deflate blocks): a screenshot needs no codec
	* @param p_path
	* @param p_width
	* @param p_height
	* @param p_rgb (8 bits per channel, rows from top to bottom)
	*/
	bool WritePng(const std::filesystem::path& p_path, uint32_t p_width, uint32_t p_height, std::span<const uint8_t> p_rgb);
}
