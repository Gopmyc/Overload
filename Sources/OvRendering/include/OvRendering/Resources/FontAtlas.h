/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

struct FT_LibraryRec_;
struct FT_FaceRec_;

namespace OvRendering::Resources
{
	/**
	* A font's glyphs at one pixel size, rasterized by FreeType into an alpha atlas in memory as the text needs them:
	* Basic Latin and Latin-1 at once, any other character the font has on demand. It knows nothing of the GPU, which
	* is what lets the tests use it
	*/
	class FontAtlas
	{
	public:
		/**
		* A glyph's metrics, in pixels, and its rectangle in the atlas, in atlas pixels: it stays where it is when the
		* atlas grows
		*/
		struct Glyph
		{
			bool valid = false;
			float xOffset = 0.0f;
			float yOffset = 0.0f;
			float xAdvance = 0.0f;
			float width = 0.0f;
			float height = 0.0f;
			float uMin = 0.0f;
			float vMin = 0.0f;
			float uMax = 0.0f;
			float vMax = 0.0f;
		};

		static constexpr uint32_t kMinimumSize = 512;
		static constexpr uint32_t kMaximumSize = 4096;

		/**
		* Opens the font at a pixel size and bakes Basic Latin and Latin-1. Returns nullptr, and why in p_error, when it
		* cannot
		* @param p_fontData (the font file's bytes, kept while the atlas lives)
		* @param p_pixelSize
		* @param p_error
		*/
		static std::unique_ptr<FontAtlas> Create(std::shared_ptr<const std::vector<uint8_t>> p_fontData, uint32_t p_pixelSize, std::string& p_error);

		~FontAtlas();

		FontAtlas(const FontAtlas&) = delete;
		FontAtlas& operator=(const FontAtlas&) = delete;

		/**
		* Bakes the characters not baked yet, those the font has. The atlas doubles when it is full, up to
		* kMaximumSize. Returns true if its pixels changed
		* @param p_codePoints
		*/
		bool Bake(std::span<const uint32_t> p_codePoints);

		/**
		* Returns a character's glyph, or nullptr when the font has none or it is not baked yet
		* @param p_codePoint
		*/
		const Glyph* GetGlyph(uint32_t p_codePoint) const;

		/**
		* Returns the atlas's width and height, in pixels
		*/
		uint32_t GetSize() const;

		/**
		* Returns the atlas's alpha, one byte a pixel, row after row from the top
		*/
		const std::vector<uint8_t>& GetPixels() const;

		float GetPixelSize() const;
		float GetLineHeight() const;
		float GetAscender() const;
		float GetDescender() const;

	private:
		FontAtlas() = default;
		bool BakeGlyph(uint32_t p_codePoint);
		bool Grow();

	private:
		std::shared_ptr<const std::vector<uint8_t>> m_fontData;
		FT_LibraryRec_* m_library = nullptr;
		FT_FaceRec_* m_face = nullptr;

		// A character the font has no glyph for keeps an invalid entry, so it is not looked for again
		std::unordered_map<uint32_t, Glyph> m_glyphs;

		std::vector<uint8_t> m_pixels;
		uint32_t m_size = 0;
		uint32_t m_cursorX = 0;
		uint32_t m_cursorY = 0;
		uint32_t m_rowHeight = 0;
		bool m_fullReported = false;

		float m_pixelSize = 0.0f;
		float m_lineHeight = 0.0f;
		float m_ascender = 0.0f;
		float m_descender = 0.0f;
	};
}
