/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cstddef>
#include <format>

#include <OvDebug/Logger.h>
#include <OvRendering/Resources/FontAtlas.h>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace
{
	constexpr uint32_t kGlyphPadding = 1;

	// Baked with the atlas, as before glyphs were baked on demand: Basic Latin, then Latin-1
	constexpr uint32_t kFirstBasicLatin = 0x20;
	constexpr uint32_t kLastBasicLatin = 0x7E;
	constexpr uint32_t kFirstLatin1 = 0xA0;
	constexpr uint32_t kLastLatin1 = 0xFF;

	bool CopyGlyphBitmap(const FT_Bitmap& p_bitmap, uint32_t p_x, uint32_t p_y, uint32_t p_atlasSize, std::vector<uint8_t>& p_pixels)
	{
		if (p_bitmap.pixel_mode != FT_PIXEL_MODE_GRAY)
		{
			return false;
		}

		const uint32_t width = static_cast<uint32_t>(p_bitmap.width);
		const uint32_t height = static_cast<uint32_t>(p_bitmap.rows);
		const int32_t pitch = static_cast<int32_t>(p_bitmap.pitch);
		const uint32_t pitchAbs = static_cast<uint32_t>(pitch >= 0 ? pitch : -pitch);

		for (uint32_t row = 0; row < height; ++row)
		{
			const uint32_t sourceRow = pitch >= 0 ? row : height - 1 - row;
			const uint8_t* source = p_bitmap.buffer + static_cast<size_t>(sourceRow) * pitchAbs;
			const size_t destination = (static_cast<size_t>(p_y) + row) * p_atlasSize + p_x;
			std::copy(source, source + width, p_pixels.begin() + destination);
		}

		return true;
	}
}

std::unique_ptr<OvRendering::Resources::FontAtlas> OvRendering::Resources::FontAtlas::Create(std::shared_ptr<const std::vector<uint8_t>> p_fontData, uint32_t p_pixelSize, std::string& p_error)
{
	if (!p_fontData || p_fontData->empty())
	{
		p_error = "no font data";
		return nullptr;
	}

	std::unique_ptr<FontAtlas> atlas(new FontAtlas());
	atlas->m_fontData = std::move(p_fontData);

	if (FT_Init_FreeType(&atlas->m_library) != 0)
	{
		atlas->m_library = nullptr;
		p_error = "unable to initialize FreeType";
		return nullptr;
	}

	// FreeType reads the bytes in place: the atlas keeps them as long as the face
	if (FT_New_Memory_Face(atlas->m_library, atlas->m_fontData->data(), static_cast<FT_Long>(atlas->m_fontData->size()), 0, &atlas->m_face) != 0)
	{
		atlas->m_face = nullptr;
		p_error = "unable to read the font";
		return nullptr;
	}

	if (FT_Select_Charmap(atlas->m_face, FT_ENCODING_UNICODE) != 0)
	{
		p_error = "the font has no Unicode character map";
		return nullptr;
	}

	if (FT_Set_Pixel_Sizes(atlas->m_face, 0, static_cast<FT_UInt>(p_pixelSize)) != 0)
	{
		p_error = std::format("unable to set the pixel size {}", p_pixelSize);
		return nullptr;
	}

	atlas->m_pixelSize = static_cast<float>(p_pixelSize);
	atlas->m_lineHeight = atlas->m_pixelSize;
	atlas->m_ascender = atlas->m_pixelSize * 0.8f;
	atlas->m_descender = -atlas->m_pixelSize * 0.2f;

	if (atlas->m_face->size)
	{
		atlas->m_lineHeight = static_cast<float>(atlas->m_face->size->metrics.height) / 64.0f;
		atlas->m_ascender = static_cast<float>(atlas->m_face->size->metrics.ascender) / 64.0f;
		atlas->m_descender = static_cast<float>(atlas->m_face->size->metrics.descender) / 64.0f;
	}

	atlas->m_size = kMinimumSize;
	atlas->m_pixels.assign(static_cast<size_t>(kMinimumSize) * kMinimumSize, 0);

	std::vector<uint32_t> latin;
	for (uint32_t codePoint = kFirstBasicLatin; codePoint <= kLastBasicLatin; ++codePoint)
	{
		latin.push_back(codePoint);
	}

	for (uint32_t codePoint = kFirstLatin1; codePoint <= kLastLatin1; ++codePoint)
	{
		latin.push_back(codePoint);
	}

	atlas->Bake(latin);
	return atlas;
}

OvRendering::Resources::FontAtlas::~FontAtlas()
{
	if (m_face)
	{
		FT_Done_Face(m_face);
	}

	if (m_library)
	{
		FT_Done_FreeType(m_library);
	}
}

bool OvRendering::Resources::FontAtlas::Bake(std::span<const uint32_t> p_codePoints)
{
	bool changed = false;

	for (const uint32_t codePoint : p_codePoints)
	{
		if (!m_glyphs.contains(codePoint))
		{
			changed = BakeGlyph(codePoint) || changed;
		}
	}

	return changed;
}

const OvRendering::Resources::FontAtlas::Glyph* OvRendering::Resources::FontAtlas::GetGlyph(uint32_t p_codePoint) const
{
	const auto found = m_glyphs.find(p_codePoint);
	return found != m_glyphs.end() && found->second.valid ? &found->second : nullptr;
}

uint32_t OvRendering::Resources::FontAtlas::GetSize() const
{
	return m_size;
}

const std::vector<uint8_t>& OvRendering::Resources::FontAtlas::GetPixels() const
{
	return m_pixels;
}

float OvRendering::Resources::FontAtlas::GetPixelSize() const
{
	return m_pixelSize;
}

float OvRendering::Resources::FontAtlas::GetLineHeight() const
{
	return m_lineHeight;
}

float OvRendering::Resources::FontAtlas::GetAscender() const
{
	return m_ascender;
}

float OvRendering::Resources::FontAtlas::GetDescender() const
{
	return m_descender;
}

bool OvRendering::Resources::FontAtlas::BakeGlyph(uint32_t p_codePoint)
{
	Glyph& glyph = m_glyphs[p_codePoint];

	if (FT_Get_Char_Index(m_face, p_codePoint) == 0 || FT_Load_Char(m_face, p_codePoint, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL) != 0)
	{
		return false;
	}

	const FT_GlyphSlot slot = m_face->glyph;
	const FT_Bitmap& bitmap = slot->bitmap;
	const uint32_t width = static_cast<uint32_t>(bitmap.width);
	const uint32_t height = static_cast<uint32_t>(bitmap.rows);

	glyph.valid = true;
	glyph.xOffset = static_cast<float>(slot->bitmap_left);
	glyph.yOffset = -static_cast<float>(slot->bitmap_top);
	glyph.xAdvance = static_cast<float>(slot->advance.x) / 64.0f;
	glyph.width = static_cast<float>(width);
	glyph.height = static_cast<float>(height);

	// A space has an advance and nothing to draw
	if (width == 0 || height == 0)
	{
		return false;
	}

	// The next shelf when the row is full, a larger atlas when the shelves are
	while (true)
	{
		if (m_cursorX + width + kGlyphPadding > m_size)
		{
			m_cursorX = 0;
			m_cursorY += m_rowHeight + kGlyphPadding;
			m_rowHeight = 0;
		}

		if (width + kGlyphPadding <= m_size && m_cursorY + height + kGlyphPadding <= m_size)
		{
			break;
		}

		if (!Grow())
		{
			if (!m_fullReported)
			{
				OVLOG_WARNING(std::format("A font atlas at {} px is full ({}x{}): characters beyond it are drawn as '?'", m_pixelSize, kMaximumSize, kMaximumSize));
				m_fullReported = true;
			}

			glyph.valid = false;
			return false;
		}
	}

	if (!CopyGlyphBitmap(bitmap, m_cursorX, m_cursorY, m_size, m_pixels))
	{
		OVLOG_WARNING(std::format("Unsupported bitmap format for the character U+{:04X}", p_codePoint));
		glyph.valid = false;
		return false;
	}

	glyph.uMin = static_cast<float>(m_cursorX);
	glyph.vMin = static_cast<float>(m_cursorY);
	glyph.uMax = static_cast<float>(m_cursorX + width);
	glyph.vMax = static_cast<float>(m_cursorY + height);

	m_cursorX += width + kGlyphPadding;
	m_rowHeight = std::max(m_rowHeight, height);
	return true;
}

bool OvRendering::Resources::FontAtlas::Grow()
{
	if (m_size >= kMaximumSize)
	{
		return false;
	}

	// The baked glyphs keep their pixels where they are, top left
	const uint32_t size = m_size * 2;
	std::vector<uint8_t> pixels(static_cast<size_t>(size) * size, 0);

	for (uint32_t row = 0; row < m_size; ++row)
	{
		const auto source = m_pixels.begin() + static_cast<ptrdiff_t>(row) * m_size;
		std::copy(source, source + m_size, pixels.begin() + static_cast<ptrdiff_t>(row) * size);
	}

	m_pixels = std::move(pixels);
	m_size = size;
	return true;
}
