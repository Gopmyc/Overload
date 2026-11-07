/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <array>
#include <fstream>
#include <string_view>
#include <vector>

#include <OvGame/Utils/PngWriter.h>

namespace
{
	uint32_t Crc32(std::span<const uint8_t> p_data, uint32_t p_crc = 0xFFFFFFFFu)
	{
		static const auto table = []
		{
			std::array<uint32_t, 256> values{};
			for (uint32_t index = 0; index < 256; ++index)
			{
				uint32_t value = index;
				for (int bit = 0; bit < 8; ++bit)
					value = (value & 1u) ? 0xEDB88320u ^ (value >> 1) : value >> 1;
				values[index] = value;
			}
			return values;
		}();

		for (const uint8_t byte : p_data)
			p_crc = table[(p_crc ^ byte) & 0xFFu] ^ (p_crc >> 8);

		return p_crc;
	}

	void AppendBigEndian(std::vector<uint8_t>& p_out, uint32_t p_value)
	{
		p_out.push_back(static_cast<uint8_t>(p_value >> 24));
		p_out.push_back(static_cast<uint8_t>(p_value >> 16));
		p_out.push_back(static_cast<uint8_t>(p_value >> 8));
		p_out.push_back(static_cast<uint8_t>(p_value));
	}

	void AppendChunk(std::vector<uint8_t>& p_out, std::string_view p_type, std::span<const uint8_t> p_data)
	{
		AppendBigEndian(p_out, static_cast<uint32_t>(p_data.size()));

		const size_t typeStart = p_out.size();
		for (const char letter : p_type)
			p_out.push_back(static_cast<uint8_t>(letter));
		p_out.insert(p_out.end(), p_data.begin(), p_data.end());

		const uint32_t crc = Crc32({ p_out.data() + typeStart, p_out.size() - typeStart }) ^ 0xFFFFFFFFu;
		AppendBigEndian(p_out, crc);
	}
}

bool OvGame::Utils::WritePng(const std::filesystem::path& p_path, uint32_t p_width, uint32_t p_height, std::span<const uint8_t> p_rgb)
{
	const size_t rowSize = static_cast<size_t>(p_width) * 3;
	if (p_width == 0 || p_height == 0 || p_rgb.size() != rowSize * p_height)
		return false;

	// Each row starts with its filter type, 0 (none)
	std::vector<uint8_t> raw;
	raw.reserve((rowSize + 1) * p_height);
	for (uint32_t row = 0; row < p_height; ++row)
	{
		raw.push_back(0);
		raw.insert(raw.end(), p_rgb.begin() + row * rowSize, p_rgb.begin() + (row + 1) * rowSize);
	}

	// A zlib stream of stored blocks of at most 65535 bytes, closed by the Adler-32 of the raw data
	std::vector<uint8_t> zlib = { 0x78, 0x01 };
	uint32_t adlerA = 1, adlerB = 0;

	for (size_t offset = 0; offset < raw.size() || offset == 0; )
	{
		const size_t size = std::min<size_t>(65535, raw.size() - offset);
		const bool last = offset + size >= raw.size();

		zlib.push_back(static_cast<uint8_t>(last ? 1 : 0));
		zlib.push_back(static_cast<uint8_t>(size));
		zlib.push_back(static_cast<uint8_t>(size >> 8));
		zlib.push_back(static_cast<uint8_t>(~size));
		zlib.push_back(static_cast<uint8_t>(~size >> 8));
		zlib.insert(zlib.end(), raw.begin() + offset, raw.begin() + offset + size);

		for (size_t index = offset; index < offset + size; ++index)
		{
			adlerA = (adlerA + raw[index]) % 65521u;
			adlerB = (adlerB + adlerA) % 65521u;
		}

		offset += size;
		if (last)
			break;
	}

	AppendBigEndian(zlib, (adlerB << 16) | adlerA);

	std::vector<uint8_t> png = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };

	std::vector<uint8_t> header;
	AppendBigEndian(header, p_width);
	AppendBigEndian(header, p_height);
	header.insert(header.end(), { 8, 2, 0, 0, 0 }); // 8 bits per channel, RGB, deflate, no filter set, no interlace

	AppendChunk(png, "IHDR", header);
	AppendChunk(png, "IDAT", zlib);
	AppendChunk(png, "IEND", {});

	std::ofstream file(p_path, std::ios::binary | std::ios::trunc);
	file.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
	return static_cast<bool>(file);
}
