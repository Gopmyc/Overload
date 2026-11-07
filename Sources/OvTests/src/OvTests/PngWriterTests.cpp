/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <OvGame/Utils/PngWriter.h>
#include <OvTests/TestUtils.h>

namespace
{
	uint32_t ReadBigEndian(const std::vector<uint8_t>& p_data, size_t p_offset)
	{
		return (uint32_t{ p_data[p_offset] } << 24) | (uint32_t{ p_data[p_offset + 1] } << 16) | (uint32_t{ p_data[p_offset + 2] } << 8) | uint32_t{ p_data[p_offset + 3] };
	}

	uint32_t Crc32(const uint8_t* p_data, size_t p_size)
	{
		uint32_t crc = 0xFFFFFFFFu;
		for (size_t index = 0; index < p_size; ++index)
		{
			crc ^= p_data[index];
			for (int bit = 0; bit < 8; ++bit)
				crc = (crc & 1u) ? 0xEDB88320u ^ (crc >> 1) : crc >> 1;
		}
		return crc ^ 0xFFFFFFFFu;
	}

	struct DecodedPng
	{
		uint32_t width = 0;
		uint32_t height = 0;
		std::vector<uint8_t> rgb;
	};

	/**
	* Reads back what WritePng writes (stored deflate blocks only), checking every CRC and the Adler-32
	*/
	std::optional<DecodedPng> ReadStoredPng(const std::filesystem::path& p_path)
	{
		std::ifstream file(p_path, std::ios::binary);
		const std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

		const std::vector<uint8_t> signature = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
		if (data.size() < 8 || !std::equal(signature.begin(), signature.end(), data.begin()))
			return std::nullopt;

		DecodedPng png;
		std::vector<uint8_t> zlib;
		bool ended = false;

		for (size_t offset = 8; offset + 12 <= data.size() && !ended; )
		{
			const uint32_t length = ReadBigEndian(data, offset);
			if (offset + 12 + length > data.size())
				return std::nullopt;

			const std::string type(data.begin() + offset + 4, data.begin() + offset + 8);
			if (Crc32(data.data() + offset + 4, length + 4) != ReadBigEndian(data, offset + 8 + length))
				return std::nullopt;

			const auto body = data.begin() + offset + 8;
			if (type == "IHDR")
			{
				png.width = ReadBigEndian(data, offset + 8);
				png.height = ReadBigEndian(data, offset + 12);
				if (body[8] != 8 || body[9] != 2 || body[12] != 0)
					return std::nullopt;
			}
			else if (type == "IDAT")
			{
				zlib.insert(zlib.end(), body, body + length);
			}
			else if (type == "IEND")
			{
				ended = true;
			}

			offset += 12 + length;
		}

		if (!ended || zlib.size() < 6 || zlib[0] != 0x78 || ((zlib[0] << 8) | zlib[1]) % 31 != 0)
			return std::nullopt;

		std::vector<uint8_t> raw;
		size_t position = 2;
		for (bool last = false; !last; )
		{
			if (position + 5 > zlib.size() || (zlib[position] & 0x06) != 0)
				return std::nullopt;

			last = (zlib[position] & 1) != 0;
			const uint16_t size = static_cast<uint16_t>(zlib[position + 1] | (zlib[position + 2] << 8));
			const uint16_t complement = static_cast<uint16_t>(zlib[position + 3] | (zlib[position + 4] << 8));
			if (static_cast<uint16_t>(~size) != complement || position + 5 + size > zlib.size())
				return std::nullopt;

			raw.insert(raw.end(), zlib.begin() + static_cast<std::ptrdiff_t>(position + 5), zlib.begin() + static_cast<std::ptrdiff_t>(position + 5 + size));
			position += 5 + size;
		}

		uint32_t a = 1, b = 0;
		for (const uint8_t byte : raw)
		{
			a = (a + byte) % 65521u;
			b = (b + a) % 65521u;
		}
		if (position + 4 != zlib.size() || ReadBigEndian(zlib, position) != ((b << 16) | a))
			return std::nullopt;

		const size_t rowSize = static_cast<size_t>(png.width) * 3;
		if (raw.size() != (rowSize + 1) * png.height)
			return std::nullopt;

		for (uint32_t row = 0; row < png.height; ++row)
		{
			const auto start = raw.begin() + static_cast<std::ptrdiff_t>(row * (rowSize + 1));
			if (*start != 0)
				return std::nullopt;
			png.rgb.insert(png.rgb.end(), start + 1, start + 1 + static_cast<std::ptrdiff_t>(rowSize));
		}

		return png;
	}

	std::vector<uint8_t> MakePixels(uint32_t p_width, uint32_t p_height)
	{
		std::vector<uint8_t> pixels(static_cast<size_t>(p_width) * p_height * 3);
		for (size_t index = 0; index < pixels.size(); ++index)
			pixels[index] = static_cast<uint8_t>((index * 7 + index / 3) & 0xFF);
		return pixels;
	}
}

TEST_SUITE("PNG writer")
{
	TEST_CASE("a small image reads back pixel for pixel")
	{
		OvTests::TempFolder folder;
		const auto pixels = MakePixels(5, 3);
		const auto path = folder.GetPath() / "small.png";

		REQUIRE(OvGame::Utils::WritePng(path, 5, 3, pixels));
		const auto png = ReadStoredPng(path);
		REQUIRE(png);
		CHECK(png->width == 5);
		CHECK(png->height == 3);
		CHECK(png->rgb == pixels);
	}

	TEST_CASE("an image larger than a deflate block spans several blocks")
	{
		OvTests::TempFolder folder;
		const auto pixels = MakePixels(300, 200); // 180 kB of rows: three stored blocks
		const auto path = folder.GetPath() / "large.png";

		REQUIRE(OvGame::Utils::WritePng(path, 300, 200, pixels));
		const auto png = ReadStoredPng(path);
		REQUIRE(png);
		CHECK(png->rgb == pixels);
	}

	TEST_CASE("what cannot be written is refused")
	{
		OvTests::TempFolder folder;
		const auto pixels = MakePixels(4, 4);

		CHECK_FALSE(OvGame::Utils::WritePng(folder.GetPath() / "empty.png", 0, 4, {}));
		CHECK_FALSE(OvGame::Utils::WritePng(folder.GetPath() / "short.png", 4, 5, pixels));
		CHECK_FALSE(OvGame::Utils::WritePng(folder.GetPath() / "missing" / "folder" / "x.png", 4, 4, pixels));
	}
}
